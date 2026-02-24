#include "eyelib/physics/PhysicsWorld.h"
#include "eyelib/physics/shapes/CollisionShapeFactory.h"
#include "eyelib/core/Log.h"

#include <unordered_map>

#include <btBulletDynamicsCommon.h>
#include <LinearMath/btIDebugDraw.h>
#include <BulletSoftBody/btSoftBody.h>
#include <BulletSoftBody/btSoftBodyHelpers.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>
#include <BulletDynamics/ConstraintSolver/btHingeConstraint.h>
#include <BulletDynamics/ConstraintSolver/btPoint2PointConstraint.h>
#include <BulletDynamics/ConstraintSolver/btSliderConstraint.h>
#include <BulletDynamics/ConstraintSolver/btFixedConstraint.h>
#include <BulletDynamics/ConstraintSolver/btGeneric6DofSpringConstraint.h>
#include <BulletDynamics/ConstraintSolver/btGeneric6DofConstraint.h>
#include <BulletDynamics/ConstraintSolver/btGearConstraint.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>

namespace eyelib {
namespace physics {

namespace {
btVector3 ToBt(const math::Vec3& v) { return btVector3(v.x, v.y, v.z); }
math::Vec3 ToGlm(const btVector3& v) { return math::Vec3(v.x(), v.y(), v.z()); }

btQuaternion ToBt(const math::Quat& q) { return btQuaternion(q.x, q.y, q.z, q.w); }
math::Quat ToGlm(const btQuaternion& q) { return math::Quat(q.w(), q.x(), q.y(), q.z()); }

btTransform ToBt(const math::Mat4& m) {
    btTransform t;
    t.setFromOpenGLMatrix(&m[0][0]);
    return t;
}

class PhysicsDebugDrawer final : public btIDebugDraw {
public:
    explicit PhysicsDebugDrawer(std::vector<DebugLine>* lines) : m_lines(lines) {}

    void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override {
        if (!m_lines) return;
        DebugLine l;
        l.from = ToGlm(from);
        l.to = ToGlm(to);
        l.color = ToGlm(color);
        m_lines->push_back(l);
    }
    void drawContactPoint(const btVector3&, const btVector3&, btScalar, int, const btVector3&) override {}
    void reportErrorWarning(const char*) override {}
    void draw3dText(const btVector3&, const char*) override {}
    void setDebugMode(int debugMode) override { m_mode = debugMode; }
    int getDebugMode() const override { return m_mode; }

private:
    std::vector<DebugLine>* m_lines = nullptr;
    int m_mode = DBG_DrawWireframe;
};
} // namespace

class PhysicsWorld::Impl {
public:
    btSoftBodyRigidBodyCollisionConfiguration* collisionConfig = nullptr;
    btCollisionDispatcher* dispatcher = nullptr;
    btBroadphaseInterface* broadphase = nullptr;
    btSequentialImpulseConstraintSolver* solver = nullptr;
    btSoftRigidDynamicsWorld* world = nullptr;
    btSoftBodyWorldInfo softBodyInfo{};
    int solverSubSteps = 3;
    int solverIterations = 10;
    PhysicsWorld::ContactCallback contactCallback;
    PhysicsWorld::ContactCallbackEx contactCallbackEx;
    std::vector<DebugLine> debugLines;
    PhysicsDebugDrawer debugDrawer{ &debugLines };
    btGhostPairCallback* ghostCallback = nullptr;

    struct BodyEntry {
        btCollisionShape* shape = nullptr;  // only used when we own it
        btDefaultMotionState* motionState = nullptr;
        btRigidBody* body = nullptr;
        bool ownShape = false;
        short group = static_cast<short>(btBroadphaseProxy::DefaultFilter);
        short mask = static_cast<short>(btBroadphaseProxy::AllFilter);
    };

    RigidBodyHandle nextHandle = 1;
    std::unordered_map<RigidBodyHandle, BodyEntry> bodies;
    std::unordered_map<const btRigidBody*, RigidBodyHandle> bodyByPtr;

    struct ConstraintEntry {
        btTypedConstraint* constraint = nullptr;
        ConstraintType type = ConstraintType::Unknown;
        RigidBodyHandle bodyA = 0;
        RigidBodyHandle bodyB = 0;
    };
    ConstraintHandle nextConstraintHandle = 1;
    std::unordered_map<ConstraintHandle, ConstraintEntry> constraints;

    CharacterControllerHandle nextControllerHandle = 1;
    struct ControllerEntry {
        btPairCachingGhostObject* ghost = nullptr;
        btKinematicCharacterController* controller = nullptr;
        btConvexShape* shape = nullptr;
        short group = static_cast<short>(btBroadphaseProxy::CharacterFilter);
        short mask = static_cast<short>(btBroadphaseProxy::AllFilter);
    };
    std::unordered_map<CharacterControllerHandle, ControllerEntry> controllers;
    std::unordered_map<const btCollisionObject*, CharacterControllerHandle> controllerByGhost;

    struct SoftBodyEntry {
        btSoftBody* body = nullptr;
        short group = static_cast<short>(btBroadphaseProxy::DefaultFilter);
        short mask = static_cast<short>(btBroadphaseProxy::AllFilter);
    };
    SoftBodyHandle nextSoftBodyHandle = 1;
    std::unordered_map<SoftBodyHandle, SoftBodyEntry> softBodies;
    std::unordered_map<const btSoftBody*, SoftBodyHandle> softBodyByPtr;

    btRigidBody* getBody(RigidBodyHandle h) const {
        auto it = bodies.find(h);
        return (it != bodies.end() && it->second.body) ? it->second.body : nullptr;
    }

    ConstraintEntry* getConstraintEntry(ConstraintHandle h) {
        auto it = constraints.find(h);
        return (it != constraints.end()) ? &it->second : nullptr;
    }
    const ConstraintEntry* getConstraintEntry(ConstraintHandle h) const {
        auto it = constraints.find(h);
        return (it != constraints.end()) ? &it->second : nullptr;
    }

    ControllerEntry* getController(CharacterControllerHandle h) {
        auto it = controllers.find(h);
        return (it != controllers.end()) ? &it->second : nullptr;
    }

    void DestroyAllBodies() {
        if (!world) return;
        for (auto& [h, e] : bodies) {
            (void)h;
            if (e.body) world->removeRigidBody(e.body);
            bodyByPtr.erase(e.body);
            delete e.body;
            delete e.motionState;
            if (e.ownShape && e.shape) delete e.shape;
        }
        bodies.clear();
    }

    void DestroyAllConstraints() {
        if (!world) return;
        for (auto& [h, e] : constraints) {
            (void)h;
            if (e.constraint) { world->removeConstraint(e.constraint); delete e.constraint; }
        }
        constraints.clear();
    }

    void DestroyAllControllers() {
        if (!world) return;
        for (auto& [h, c] : controllers) {
            (void)h;
            if (c.controller) world->removeAction(c.controller);
            if (c.ghost) world->removeCollisionObject(c.ghost);
            delete c.controller;
            delete c.ghost;
            delete c.shape;
        }
        controllers.clear();
        controllerByGhost.clear();
    }

    void DestroyAllSoftBodies() {
        if (!world) return;
        for (auto& [h, e] : softBodies) {
            (void)h;
            if (e.body) {
                world->removeSoftBody(e.body);
                delete e.body;
            }
        }
        softBodies.clear();
        softBodyByPtr.clear();
    }

    static void InternalTickCallback(btDynamicsWorld* w, btScalar) {
        if (!w) return;
        auto* impl = static_cast<Impl*>(w->getWorldUserInfo());
        if (!impl || (!impl->contactCallback && !impl->contactCallbackEx)) return;
        btDispatcher* dispatcher = w->getDispatcher();
        if (!dispatcher) return;
        const int numManifolds = dispatcher->getNumManifolds();
        for (int i = 0; i < numManifolds; ++i) {
            btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
            if (!manifold || manifold->getNumContacts() == 0) continue;
            const btCollisionObject* a = manifold->getBody0();
            const btCollisionObject* b = manifold->getBody1();
            const btRigidBody* ra = btRigidBody::upcast(a);
            const btRigidBody* rb = btRigidBody::upcast(b);
            RigidBodyHandle ha = 0;
            RigidBodyHandle hb = 0;
            if (ra) {
                auto ita = impl->bodyByPtr.find(ra);
                if (ita != impl->bodyByPtr.end()) ha = ita->second;
            }
            if (rb) {
                auto itb = impl->bodyByPtr.find(rb);
                if (itb != impl->bodyByPtr.end()) hb = itb->second;
            }
            const btManifoldPoint& cp = manifold->getContactPoint(0);
            math::Vec3 point = ToGlm(cp.m_positionWorldOnB);
            math::Vec3 normal = ToGlm(cp.m_normalWorldOnB);
            if (impl->contactCallback && (ha != 0 || hb != 0))
                impl->contactCallback(ha, hb, point, normal);

            if (impl->contactCallbackEx) {
                ContactObject oa{};
                ContactObject ob{};
                if (ra) { oa.type = ContactObjectType::RigidBody; oa.handle = ha; }
                if (rb) { ob.type = ContactObjectType::RigidBody; ob.handle = hb; }

                const btSoftBody* sa = btSoftBody::upcast(a);
                const btSoftBody* sb = btSoftBody::upcast(b);
                if (sa) {
                    auto its = impl->softBodyByPtr.find(sa);
                    if (its != impl->softBodyByPtr.end()) {
                        oa.type = ContactObjectType::SoftBody;
                        oa.handle = its->second;
                    }
                }
                if (sb) {
                    auto its = impl->softBodyByPtr.find(sb);
                    if (its != impl->softBodyByPtr.end()) {
                        ob.type = ContactObjectType::SoftBody;
                        ob.handle = its->second;
                    }
                }

                auto itga = impl->controllerByGhost.find(a);
                if (itga != impl->controllerByGhost.end()) {
                    oa.type = ContactObjectType::CharacterController;
                    oa.handle = itga->second;
                }
                auto itgb = impl->controllerByGhost.find(b);
                if (itgb != impl->controllerByGhost.end()) {
                    ob.type = ContactObjectType::CharacterController;
                    ob.handle = itgb->second;
                }

                if (oa.type != ContactObjectType::Unknown || ob.type != ContactObjectType::Unknown)
                    impl->contactCallbackEx(oa, ob, point, normal);
            }
        }
    }
};

PhysicsWorld::PhysicsWorld() : m_impl(new Impl()) {}

PhysicsWorld::~PhysicsWorld() {
    Shutdown();
    delete m_impl;
    m_impl = nullptr;
}

bool PhysicsWorld::Initialize(const math::Vec3& gravity) {
    if (!m_impl) return false;
    if (m_impl->world) {
        core::Log::Warn("PhysicsWorld already initialized");
        return true;
    }

    m_impl->collisionConfig = new btSoftBodyRigidBodyCollisionConfiguration();
    m_impl->dispatcher = new btCollisionDispatcher(m_impl->collisionConfig);
    m_impl->broadphase = new btDbvtBroadphase();
    m_impl->solver = new btSequentialImpulseConstraintSolver();
    m_impl->world = new btSoftRigidDynamicsWorld(
        m_impl->dispatcher,
        m_impl->broadphase,
        m_impl->solver,
        m_impl->collisionConfig
    );

    m_impl->ghostCallback = new btGhostPairCallback();
    m_impl->world->getBroadphase()->getOverlappingPairCache()->setInternalGhostPairCallback(m_impl->ghostCallback);

    m_impl->world->getSolverInfo().m_numIterations = m_impl->solverIterations;
    m_impl->world->setDebugDrawer(&m_impl->debugDrawer);
    m_impl->world->setWorldUserInfo(m_impl);
    m_impl->world->setInternalTickCallback(&Impl::InternalTickCallback, m_impl, true);

    m_impl->softBodyInfo.m_broadphase = m_impl->broadphase;
    m_impl->softBodyInfo.m_dispatcher = m_impl->dispatcher;
    m_impl->softBodyInfo.m_gravity = ToBt(gravity);
    m_impl->softBodyInfo.m_sparsesdf.Initialize();

    m_impl->world->setGravity(ToBt(gravity));
    core::Log::Info("PhysicsWorld initialized");
    return true;
}

void PhysicsWorld::Shutdown() {
    if (!m_impl) return;
    if (!m_impl->world) return;

    m_impl->DestroyAllSoftBodies();
    m_impl->DestroyAllControllers();
    m_impl->DestroyAllConstraints();
    m_impl->DestroyAllBodies();

    delete m_impl->ghostCallback; m_impl->ghostCallback = nullptr;
    delete m_impl->world; m_impl->world = nullptr;
    delete m_impl->solver; m_impl->solver = nullptr;
    delete m_impl->broadphase; m_impl->broadphase = nullptr;
    delete m_impl->dispatcher; m_impl->dispatcher = nullptr;
    delete m_impl->collisionConfig; m_impl->collisionConfig = nullptr;

    core::Log::Info("PhysicsWorld shutdown");
}

void PhysicsWorld::Step(float deltaTime, int maxSubSteps, float fixedTimeStep) {
    if (!m_impl || !m_impl->world) return;
    const float clamped = (deltaTime < 0.0f) ? 0.0f : (deltaTime > 0.1f ? 0.1f : deltaTime);
    const int subSteps = (maxSubSteps > 0) ? maxSubSteps : m_impl->solverSubSteps;
    const float step = (fixedTimeStep > 0.0f) ? fixedTimeStep : (1.0f / 60.0f);
    m_impl->world->stepSimulation(clamped, subSteps, step);
}

RigidBodyHandle PhysicsWorld::CreateBoxRigidBody(float mass, const math::Vec3& halfExtents, const math::Vec3& position) {
    CollisionShapeHandle sh = CollisionShapeFactory::CreateBox(halfExtents);
    return CreateRigidBody(position, math::Quat(1.f, 0.f, 0.f, 0.f), sh, mass, math::Vec3(0.f));
}

RigidBodyHandle PhysicsWorld::CreateRigidBody(const math::Vec3& position, const math::Quat& rotation,
                                             CollisionShapeHandle shapeHandle, float mass,
                                             const math::Vec3& inertia) {
    if (!m_impl || !m_impl->world) return 0;
    btCollisionShape* shape = CollisionShapeFactory::GetShape(shapeHandle);
    if (!shape) {
        core::Log::Warn("PhysicsWorld::CreateRigidBody: invalid shape handle");
        return 0;
    }

    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(ToBt(position));
    startTransform.setRotation(ToBt(rotation));

    btVector3 localInertia(0, 0, 0);
    if (mass > 0.f) {
        if (inertia.x > 0.f || inertia.y > 0.f || inertia.z > 0.f)
            localInertia = ToBt(inertia);
        else
            shape->calculateLocalInertia(mass, localInertia);
    }

    btDefaultMotionState* motionState = new btDefaultMotionState(startTransform);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, localInertia);
    btRigidBody* body = new btRigidBody(rbInfo);
    m_impl->world->addRigidBody(body, btBroadphaseProxy::DefaultFilter, btBroadphaseProxy::AllFilter);

    RigidBodyHandle handle = m_impl->nextHandle++;
    Impl::BodyEntry e;
    e.shape = nullptr;
    e.motionState = motionState;
    e.body = body;
    e.ownShape = false;
    e.group = static_cast<short>(btBroadphaseProxy::DefaultFilter);
    e.mask = static_cast<short>(btBroadphaseProxy::AllFilter);
    m_impl->bodies.emplace(handle, std::move(e));
    m_impl->bodyByPtr[body] = handle;
    return handle;
}

void PhysicsWorld::DestroyRigidBody(RigidBodyHandle body) {
    if (!m_impl || !m_impl->world) return;
    auto it = m_impl->bodies.find(body);
    if (it == m_impl->bodies.end()) return;

    // Remove any constraints that reference this body to avoid dangling pointers.
    std::vector<ConstraintHandle> toRemove;
    toRemove.reserve(m_impl->constraints.size());
    for (const auto& [h, e] : m_impl->constraints) {
        if (e.bodyA == body || e.bodyB == body) toRemove.push_back(h);
    }
    for (ConstraintHandle h : toRemove) DestroyConstraint(h);

    auto& e = it->second;
    if (e.body) {
        m_impl->world->removeRigidBody(e.body);
        m_impl->bodyByPtr.erase(e.body);
    }
    delete e.body;
    delete e.motionState;
    if (e.ownShape && e.shape) delete e.shape;
    m_impl->bodies.erase(it);
}

std::optional<RigidBodyTransform> PhysicsWorld::GetRigidBodyTransform(RigidBodyHandle body) const {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (!rb || !rb->getMotionState()) return std::nullopt;
    btTransform t;
    rb->getMotionState()->getWorldTransform(t);
    RigidBodyTransform out;
    out.position = ToGlm(t.getOrigin());
    out.rotation = ToGlm(t.getRotation());
    return out;
}

bool PhysicsWorld::SetRigidBodyTransform(RigidBodyHandle body, const math::Vec3& position, const math::Quat& rotation) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (!rb) return false;
    btTransform t;
    t.setIdentity();
    t.setOrigin(ToBt(position));
    t.setRotation(ToBt(rotation));
    rb->setWorldTransform(t);
    if (rb->getMotionState()) rb->getMotionState()->setWorldTransform(t);
    rb->activate(true);
    return true;
}

void PhysicsWorld::SetVelocity(RigidBodyHandle body, const math::Vec3& velocity) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (rb) rb->setLinearVelocity(ToBt(velocity));
}

math::Vec3 PhysicsWorld::GetVelocity(RigidBodyHandle body) const {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    return rb ? ToGlm(rb->getLinearVelocity()) : math::Vec3(0.f);
}

void PhysicsWorld::SetAngularVelocity(RigidBodyHandle body, const math::Vec3& omega) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (rb) rb->setAngularVelocity(ToBt(omega));
}

math::Vec3 PhysicsWorld::GetAngularVelocity(RigidBodyHandle body) const {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    return rb ? ToGlm(rb->getAngularVelocity()) : math::Vec3(0.f);
}

void PhysicsWorld::ApplyForce(RigidBodyHandle body, const math::Vec3& force, const math::Vec3& point) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (rb) rb->applyForce(ToBt(force), ToBt(point));
}

void PhysicsWorld::ApplyImpulse(RigidBodyHandle body, const math::Vec3& impulse, const math::Vec3& point) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (rb) rb->applyImpulse(ToBt(impulse), ToBt(point));
}

void PhysicsWorld::SetMass(RigidBodyHandle body, float mass) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (!rb) return;
    btVector3 inertia(0, 0, 0);
    if (mass > 0.f && rb->getCollisionShape())
        rb->getCollisionShape()->calculateLocalInertia(mass, inertia);
    rb->setMassProps(mass, inertia);
}

float PhysicsWorld::GetMass(RigidBodyHandle body) const {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (!rb) return 0.f;
    btScalar inv = rb->getInvMass();
    return (inv > 0.f) ? (1.f / inv) : 0.f;
}

void PhysicsWorld::SetFriction(RigidBodyHandle body, float friction) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (rb) rb->setFriction(friction);
}

void PhysicsWorld::SetRestitution(RigidBodyHandle body, float restitution) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (rb) rb->setRestitution(restitution);
}

void PhysicsWorld::SetLinearDamping(RigidBodyHandle body, float damping) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (rb) rb->setDamping(damping, rb->getAngularDamping());
}

void PhysicsWorld::SetAngularDamping(RigidBodyHandle body, float damping) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (rb) rb->setDamping(rb->getLinearDamping(), damping);
}

void PhysicsWorld::SetKinematic(RigidBodyHandle body, bool kinematic) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (!rb) return;
    if (kinematic) {
        rb->setCollisionFlags(rb->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        rb->setActivationState(DISABLE_DEACTIVATION);
    } else {
        rb->setCollisionFlags(rb->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        rb->setActivationState(ACTIVE_TAG);
    }
}

bool PhysicsWorld::IsKinematic(RigidBodyHandle body) const {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    return rb ? (rb->getCollisionFlags() & btCollisionObject::CF_KINEMATIC_OBJECT) != 0 : false;
}

void PhysicsWorld::SetRigidBodyGravity(RigidBodyHandle body, const math::Vec3& gravity) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (rb) rb->setGravity(ToBt(gravity));
}

void PhysicsWorld::SetSolverIterations(int iterations) {
    if (!m_impl || !m_impl->world) return;
    m_impl->solverIterations = iterations;
    m_impl->world->getSolverInfo().m_numIterations = iterations;
}

void PhysicsWorld::SetSolverSubSteps(int subSteps) {
    if (!m_impl) return;
    m_impl->solverSubSteps = subSteps;
}

void PhysicsWorld::SetContactCallback(std::function<void(RigidBodyHandle, RigidBodyHandle, const math::Vec3&, const math::Vec3&)> cb) {
    if (!m_impl || !m_impl->world) return;
    m_impl->contactCallback = std::move(cb);
    m_impl->world->setWorldUserInfo(m_impl);
    m_impl->world->setInternalTickCallback(&Impl::InternalTickCallback, m_impl, true);
}

void PhysicsWorld::SetContactCallbackEx(ContactCallbackEx cb) {
    if (!m_impl || !m_impl->world) return;
    m_impl->contactCallbackEx = std::move(cb);
    m_impl->world->setWorldUserInfo(m_impl);
    m_impl->world->setInternalTickCallback(&Impl::InternalTickCallback, m_impl, true);
}

void PhysicsWorld::SetCollisionFilter(RigidBodyHandle body, short group, short mask) {
    if (!m_impl || !m_impl->world) return;
    auto it = m_impl->bodies.find(body);
    if (it == m_impl->bodies.end() || !it->second.body) return;
    btRigidBody* rb = it->second.body;
    m_impl->world->removeRigidBody(rb);
    it->second.group = group;
    it->second.mask = mask;
    m_impl->world->addRigidBody(rb, group, mask);
}

size_t PhysicsWorld::GetBodyCount() const {
    return m_impl ? m_impl->bodies.size() : 0;
}

size_t PhysicsWorld::GetConstraintCount() const {
    return m_impl ? m_impl->constraints.size() : 0;
}

void PhysicsWorld::CollectDebugDrawData() {
    if (!m_impl || !m_impl->world) return;
    m_impl->debugLines.clear();
    m_impl->world->debugDrawWorld();
}

const std::vector<DebugLine>& PhysicsWorld::GetDebugDrawData() const {
    static std::vector<DebugLine> empty;
    return m_impl ? m_impl->debugLines : empty;
}

void PhysicsWorld::SetGravity(const math::Vec3& gravity) {
    if (!m_impl || !m_impl->world) return;
    m_impl->world->setGravity(ToBt(gravity));
}

math::Vec3 PhysicsWorld::GetGravity() const {
    if (!m_impl || !m_impl->world) return math::Vec3(0.0f, -9.81f, 0.0f);
    return ToGlm(m_impl->world->getGravity());
}

PhysicsWorld::ConstraintHandle PhysicsWorld::CreateHingeConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Vec3& pivotA, const math::Vec3& pivotB,
    const math::Vec3& axisA, const math::Vec3& axisB) {
    btRigidBody* ra = m_impl ? m_impl->getBody(bodyA) : nullptr;
    btRigidBody* rb = m_impl ? m_impl->getBody(bodyB) : nullptr;
    if (!ra || !rb || !m_impl->world) return 0;
    auto* c = new btHingeConstraint(*ra, *rb, ToBt(pivotA), ToBt(pivotB), ToBt(axisA), ToBt(axisB));
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::Hinge, bodyA, bodyB };
    return h;
}

PhysicsWorld::ConstraintHandle PhysicsWorld::CreateHingeConstraintWorld(RigidBodyHandle body,
    const math::Vec3& pivot, const math::Vec3& axis) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (!rb || !m_impl->world) return 0;
    auto* c = new btHingeConstraint(*rb, ToBt(pivot), ToBt(axis));
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::Hinge, body, 0 };
    return h;
}

PhysicsWorld::ConstraintHandle PhysicsWorld::CreateBallSocketConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Vec3& pivotA, const math::Vec3& pivotB) {
    btRigidBody* ra = m_impl ? m_impl->getBody(bodyA) : nullptr;
    btRigidBody* rb = m_impl ? m_impl->getBody(bodyB) : nullptr;
    if (!ra || !rb || !m_impl->world) return 0;
    auto* c = new btPoint2PointConstraint(*ra, *rb, ToBt(pivotA), ToBt(pivotB));
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::BallSocket, bodyA, bodyB };
    return h;
}

PhysicsWorld::ConstraintHandle PhysicsWorld::CreateSliderConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Mat4& frameA, const math::Mat4& frameB) {
    btRigidBody* ra = m_impl ? m_impl->getBody(bodyA) : nullptr;
    btRigidBody* rb = m_impl ? m_impl->getBody(bodyB) : nullptr;
    if (!ra || !rb || !m_impl->world) return 0;
    auto* c = new btSliderConstraint(*ra, *rb, ToBt(frameA), ToBt(frameB), true);
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::Slider, bodyA, bodyB };
    return h;
}

PhysicsWorld::ConstraintHandle PhysicsWorld::CreateSliderConstraintWorld(RigidBodyHandle body, const math::Mat4& frame) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (!rb || !m_impl->world) return 0;
    auto* c = new btSliderConstraint(*rb, ToBt(frame), true);
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::Slider, body, 0 };
    return h;
}

PhysicsWorld::ConstraintHandle PhysicsWorld::CreateFixedConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Mat4& frameA, const math::Mat4& frameB) {
    btRigidBody* ra = m_impl ? m_impl->getBody(bodyA) : nullptr;
    btRigidBody* rb = m_impl ? m_impl->getBody(bodyB) : nullptr;
    if (!ra || !rb || !m_impl->world) return 0;
    auto* c = new btFixedConstraint(*ra, *rb, ToBt(frameA), ToBt(frameB));
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::Fixed, bodyA, bodyB };
    return h;
}

PhysicsWorld::ConstraintHandle PhysicsWorld::CreateFixedConstraintWorld(RigidBodyHandle body, const math::Mat4& frame) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (!rb || !m_impl->world) return 0;
    auto* c = new btGeneric6DofConstraint(*rb, ToBt(frame), true);
    c->setLinearLowerLimit(btVector3(0, 0, 0));
    c->setLinearUpperLimit(btVector3(0, 0, 0));
    c->setAngularLowerLimit(btVector3(0, 0, 0));
    c->setAngularUpperLimit(btVector3(0, 0, 0));
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::Fixed, body, 0 };
    return h;
}

PhysicsWorld::ConstraintHandle PhysicsWorld::CreateSpringConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Vec3& pivotA, const math::Vec3& pivotB, float stiffness, float damping) {
    btRigidBody* ra = m_impl ? m_impl->getBody(bodyA) : nullptr;
    btRigidBody* rb = m_impl ? m_impl->getBody(bodyB) : nullptr;
    if (!ra || !rb || !m_impl->world) return 0;
    btTransform fa, fb;
    fa.setIdentity(); fa.setOrigin(ToBt(pivotA));
    fb.setIdentity(); fb.setOrigin(ToBt(pivotB));
    auto* c = new btGeneric6DofSpringConstraint(*ra, *rb, fa, fb, true);
    for (int i = 0; i < 3; ++i) {
        c->enableSpring(i, true);
        c->setStiffness(i, stiffness);
        c->setDamping(i, damping);
        c->setEquilibriumPoint(i);
    }
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::Spring, bodyA, bodyB };
    return h;
}

PhysicsWorld::ConstraintHandle PhysicsWorld::Create6DOFConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Mat4& frameA, const math::Mat4& frameB) {
    btRigidBody* ra = m_impl ? m_impl->getBody(bodyA) : nullptr;
    btRigidBody* rb = m_impl ? m_impl->getBody(bodyB) : nullptr;
    if (!ra || !rb || !m_impl->world) return 0;
    auto* c = new btGeneric6DofConstraint(*ra, *rb, ToBt(frameA), ToBt(frameB), true);
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::SixDof, bodyA, bodyB };
    return h;
}

PhysicsWorld::ConstraintHandle PhysicsWorld::Create6DOFConstraintWorld(RigidBodyHandle body, const math::Mat4& frame) {
    btRigidBody* rb = m_impl ? m_impl->getBody(body) : nullptr;
    if (!rb || !m_impl->world) return 0;
    auto* c = new btGeneric6DofConstraint(*rb, ToBt(frame), true);
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::SixDof, body, 0 };
    return h;
}

PhysicsWorld::ConstraintHandle PhysicsWorld::CreateGearConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Vec3& axisA, const math::Vec3& axisB, float ratio) {
    btRigidBody* ra = m_impl ? m_impl->getBody(bodyA) : nullptr;
    btRigidBody* rb = m_impl ? m_impl->getBody(bodyB) : nullptr;
    if (!ra || !rb || !m_impl->world) return 0;
    auto* c = new btGearConstraint(*ra, *rb, ToBt(axisA), ToBt(axisB), ratio);
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::Gear, bodyA, bodyB };
    return h;
}

PhysicsWorld::ConstraintHandle PhysicsWorld::CreateRopeConstraint(RigidBodyHandle bodyA, RigidBodyHandle bodyB,
    const math::Vec3& pivotA, const math::Vec3& pivotB, float length) {
    btRigidBody* ra = m_impl ? m_impl->getBody(bodyA) : nullptr;
    btRigidBody* rb = m_impl ? m_impl->getBody(bodyB) : nullptr;
    if (!ra || !rb || !m_impl->world) return 0;
    btTransform fa, fb;
    fa.setIdentity(); fa.setOrigin(ToBt(pivotA));
    fb.setIdentity(); fb.setOrigin(ToBt(pivotB));
    auto* c = new btGeneric6DofConstraint(*ra, *rb, fa, fb, true);
    const btScalar l = btScalar(length);
    c->setLinearLowerLimit(btVector3(-l, -l, -l));
    c->setLinearUpperLimit(btVector3( l,  l,  l));
    c->setAngularLowerLimit(btVector3(0, 0, 0));
    c->setAngularUpperLimit(btVector3(0, 0, 0));
    m_impl->world->addConstraint(c, true);
    ConstraintHandle h = m_impl->nextConstraintHandle++;
    m_impl->constraints[h] = { c, ConstraintType::Rope, bodyA, bodyB };
    return h;
}

void PhysicsWorld::DestroyConstraint(ConstraintHandle h) {
    if (!m_impl || !m_impl->world) return;
    auto it = m_impl->constraints.find(h);
    if (it == m_impl->constraints.end()) return;
    btTypedConstraint* c = it->second.constraint;
    if (c) m_impl->world->removeConstraint(c);
    delete c;
    m_impl->constraints.erase(it);
}

void PhysicsWorld::SetHingeLimits(ConstraintHandle h, float lower, float upper) {
    auto* entry = m_impl ? m_impl->getConstraintEntry(h) : nullptr;
    if (!entry || !entry->constraint || entry->type != ConstraintType::Hinge) return;
    auto* hinge = static_cast<btHingeConstraint*>(entry->constraint);
    hinge->setLimit(lower, upper);
}

void PhysicsWorld::SetHingeMotor(ConstraintHandle h, bool enabled, float targetVelocity, float maxForce) {
    auto* entry = m_impl ? m_impl->getConstraintEntry(h) : nullptr;
    if (!entry || !entry->constraint || entry->type != ConstraintType::Hinge) return;
    auto* hinge = static_cast<btHingeConstraint*>(entry->constraint);
    hinge->enableAngularMotor(enabled, targetVelocity, maxForce);
}

void PhysicsWorld::SetSliderLimits(ConstraintHandle h, float lowerLin, float upperLin, float lowerAng, float upperAng) {
    auto* entry = m_impl ? m_impl->getConstraintEntry(h) : nullptr;
    if (!entry || !entry->constraint || entry->type != ConstraintType::Slider) return;
    auto* slider = static_cast<btSliderConstraint*>(entry->constraint);
    slider->setLowerLinLimit(lowerLin);
    slider->setUpperLinLimit(upperLin);
    slider->setLowerAngLimit(lowerAng);
    slider->setUpperAngLimit(upperAng);
}

void PhysicsWorld::SetSliderMotor(ConstraintHandle h, bool linearEnabled, float targetVelocity, float maxForce,
                                  bool angularEnabled, float targetAngVelocity, float maxAngForce) {
    auto* entry = m_impl ? m_impl->getConstraintEntry(h) : nullptr;
    if (!entry || !entry->constraint || entry->type != ConstraintType::Slider) return;
    auto* slider = static_cast<btSliderConstraint*>(entry->constraint);
    slider->setPoweredLinMotor(linearEnabled);
    slider->setTargetLinMotorVelocity(targetVelocity);
    slider->setMaxLinMotorForce(maxForce);
    slider->setPoweredAngMotor(angularEnabled);
    slider->setTargetAngMotorVelocity(targetAngVelocity);
    slider->setMaxAngMotorForce(maxAngForce);
}

void PhysicsWorld::Set6DOFLimits(ConstraintHandle h, const math::Vec3& linearLower, const math::Vec3& linearUpper,
                                 const math::Vec3& angularLower, const math::Vec3& angularUpper) {
    auto* entry = m_impl ? m_impl->getConstraintEntry(h) : nullptr;
    if (!entry || !entry->constraint ||
        (entry->type != ConstraintType::SixDof && entry->type != ConstraintType::Fixed && entry->type != ConstraintType::Rope)) return;
    auto* dof = static_cast<btGeneric6DofConstraint*>(entry->constraint);
    dof->setLinearLowerLimit(ToBt(linearLower));
    dof->setLinearUpperLimit(ToBt(linearUpper));
    dof->setAngularLowerLimit(ToBt(angularLower));
    dof->setAngularUpperLimit(ToBt(angularUpper));
}

void PhysicsWorld::Set6DOFSpring(ConstraintHandle h, int axis, bool enabled, float stiffness, float damping) {
    auto* entry = m_impl ? m_impl->getConstraintEntry(h) : nullptr;
    if (!entry || !entry->constraint || entry->type != ConstraintType::Spring || axis < 0 || axis > 5) return;
    auto* spring = static_cast<btGeneric6DofSpringConstraint*>(entry->constraint);
    spring->enableSpring(axis, enabled);
    spring->setStiffness(axis, stiffness);
    spring->setDamping(axis, damping);
}

void PhysicsWorld::Set6DOFEquilibrium(ConstraintHandle h, int axis) {
    auto* entry = m_impl ? m_impl->getConstraintEntry(h) : nullptr;
    if (!entry || !entry->constraint || entry->type != ConstraintType::Spring) return;
    auto* spring = static_cast<btGeneric6DofSpringConstraint*>(entry->constraint);
    if (axis < 0) {
        spring->setEquilibriumPoint();
    } else if (axis <= 5) {
        spring->setEquilibriumPoint(axis);
    }
}

void PhysicsWorld::SetConstraintBreakingImpulseThreshold(ConstraintHandle h, float threshold) {
    auto* entry = m_impl ? m_impl->getConstraintEntry(h) : nullptr;
    if (!entry || !entry->constraint) return;
    entry->constraint->setBreakingImpulseThreshold(threshold);
}

PhysicsWorld::ConstraintType PhysicsWorld::GetConstraintType(ConstraintHandle h) const {
    auto* entry = m_impl ? m_impl->getConstraintEntry(h) : nullptr;
    return entry ? entry->type : ConstraintType::Unknown;
}

bool PhysicsWorld::GetConstraintBodies(ConstraintHandle h, RigidBodyHandle& bodyA, RigidBodyHandle& bodyB) const {
    auto* entry = m_impl ? m_impl->getConstraintEntry(h) : nullptr;
    if (!entry) return false;
    bodyA = entry->bodyA;
    bodyB = entry->bodyB;
    return true;
}

PhysicsWorld::CharacterControllerHandle PhysicsWorld::CreateCharacterController(const math::Vec3& position, float radius, float height) {
    if (!m_impl || !m_impl->world) return 0;
    auto* shape = new btCapsuleShape(radius, height);
    auto* ghost = new btPairCachingGhostObject();
    ghost->setCollisionShape(shape);
    ghost->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

    btTransform t;
    t.setIdentity();
    t.setOrigin(ToBt(position));
    ghost->setWorldTransform(t);

    const btScalar stepHeight = btScalar(0.35f);
    // Up = Y axis (default is X, which causes left drift). Gravity is applied along -up.
    const btVector3 up(0, 1, 0);
    auto* controller = new btKinematicCharacterController(ghost, shape, stepHeight, up);
    controller->setGravity(m_impl->world->getGravity());

    CharacterControllerHandle h = m_impl->nextControllerHandle++;
    Impl::ControllerEntry e;
    e.ghost = ghost;
    e.controller = controller;
    e.shape = shape;
    m_impl->controllers.emplace(h, std::move(e));
    m_impl->controllerByGhost[ghost] = h;

    m_impl->world->addCollisionObject(ghost, e.group, e.mask);
    m_impl->world->addAction(controller);
    return h;
}

void PhysicsWorld::DestroyCharacterController(CharacterControllerHandle h) {
    if (!m_impl || !m_impl->world) return;
    auto it = m_impl->controllers.find(h);
    if (it == m_impl->controllers.end()) return;
    auto& e = it->second;
    if (e.controller) m_impl->world->removeAction(e.controller);
    if (e.ghost) m_impl->world->removeCollisionObject(e.ghost);
    if (e.ghost) m_impl->controllerByGhost.erase(e.ghost);
    delete e.controller;
    delete e.ghost;
    delete e.shape;
    m_impl->controllers.erase(it);
}

void PhysicsWorld::MoveCharacterController(CharacterControllerHandle h, const math::Vec3& direction, float deltaTime) {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (!e || !e->controller) return;
    e->controller->setWalkDirection(ToBt(direction) * deltaTime);
}

void PhysicsWorld::SetCharacterPosition(CharacterControllerHandle h, const math::Vec3& position) {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (!e || !e->ghost) return;
    btTransform t = e->ghost->getWorldTransform();
    t.setOrigin(ToBt(position));
    e->ghost->setWorldTransform(t);
    if (e->controller) e->controller->warp(ToBt(position));
}

math::Vec3 PhysicsWorld::GetCharacterPosition(CharacterControllerHandle h) const {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (!e || !e->ghost) return math::Vec3(0.f);
    return ToGlm(e->ghost->getWorldTransform().getOrigin());
}

void PhysicsWorld::SetCharacterRotation(CharacterControllerHandle h, const math::Quat& rotation) {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (!e || !e->ghost) return;
    btTransform t = e->ghost->getWorldTransform();
    t.setRotation(ToBt(rotation));
    e->ghost->setWorldTransform(t);
}

math::Quat PhysicsWorld::GetCharacterRotation(CharacterControllerHandle h) const {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (!e || !e->ghost) return math::Quat(1.f, 0.f, 0.f, 0.f);
    return ToGlm(e->ghost->getWorldTransform().getRotation());
}

math::Vec3 PhysicsWorld::GetCharacterVelocity(CharacterControllerHandle h) const {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (!e || !e->ghost) return math::Vec3(0.f);
    return ToGlm(e->ghost->getInterpolationLinearVelocity());
}

bool PhysicsWorld::IsCharacterOnGround(CharacterControllerHandle h) const {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    return (e && e->controller) ? e->controller->onGround() : false;
}

math::Vec3 PhysicsWorld::GetCharacterGroundNormal(CharacterControllerHandle h) const {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (!e || !e->controller || !m_impl || !m_impl->world) return math::Vec3(0.f, 1.f, 0.f);
    const btVector3 up = e->controller->getUp();
    if (!e->controller->onGround()) return ToGlm(up);
    const btVector3 pos = e->ghost->getWorldTransform().getOrigin();
    const btVector3 from = pos + up * btScalar(0.1f);
    const btVector3 to = pos - up * btScalar(3.0f);
    struct RayCallback final : public btCollisionWorld::ClosestRayResultCallback {
        const btCollisionObject* ignore;
        RayCallback(const btVector3& f, const btVector3& t, const btCollisionObject* ig)
            : btCollisionWorld::ClosestRayResultCallback(f, t), ignore(ig) {}
        btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
            if (rayResult.m_collisionObject == ignore) return btScalar(1.0);
            return btCollisionWorld::ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
        }
    };
    RayCallback cb(from, to, e->ghost);
    cb.m_collisionFilterGroup = e->group;
    cb.m_collisionFilterMask = e->mask;
    m_impl->world->rayTest(from, to, cb);
    return cb.hasHit() ? ToGlm(cb.m_hitNormalWorld) : ToGlm(up);
}

void PhysicsWorld::SetCharacterStepHeight(CharacterControllerHandle h, float height) {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (e && e->controller) e->controller->setStepHeight(height);
}

void PhysicsWorld::SetCharacterSlopeLimit(CharacterControllerHandle h, float radians) {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (e && e->controller) e->controller->setMaxSlope(radians);
}

void PhysicsWorld::SetCharacterJumpSpeed(CharacterControllerHandle h, float speed) {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (e && e->controller) e->controller->setJumpSpeed(speed);
}

void PhysicsWorld::SetCharacterFallSpeed(CharacterControllerHandle h, float speed) {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (e && e->controller) e->controller->setFallSpeed(speed);
}

void PhysicsWorld::SetCharacterMaxJumpHeight(CharacterControllerHandle h, float height) {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (e && e->controller) e->controller->setMaxJumpHeight(height);
}

void PhysicsWorld::SetCharacterGravity(CharacterControllerHandle h, const math::Vec3& gravity) {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (e && e->controller) e->controller->setGravity(ToBt(gravity));
}

void PhysicsWorld::JumpCharacter(CharacterControllerHandle h) {
    auto* e = m_impl ? m_impl->getController(h) : nullptr;
    if (e && e->controller) e->controller->jump();
}

void PhysicsWorld::SetCharacterCollisionFilter(CharacterControllerHandle h, short group, short mask) {
    if (!m_impl || !m_impl->world) return;
    auto it = m_impl->controllers.find(h);
    if (it == m_impl->controllers.end()) return;
    auto& e = it->second;
    if (!e.ghost) return;
    m_impl->world->removeCollisionObject(e.ghost);
    e.group = group;
    e.mask = mask;
    m_impl->world->addCollisionObject(e.ghost, group, mask);
}

btDynamicsWorld* PhysicsWorld::GetWorldInternal() {
    return m_impl ? m_impl->world : nullptr;
}

RigidBodyHandle PhysicsWorld::GetHandleFromCollisionObject(const btCollisionObject* obj) const {
    if (!m_impl || !obj) return 0;
    const btRigidBody* rb = btRigidBody::upcast(obj);
    if (!rb) return 0;
    auto it = m_impl->bodyByPtr.find(rb);
    return (it != m_impl->bodyByPtr.end()) ? it->second : 0;
}

btRigidBody* PhysicsWorld::GetRigidBodyInternal(RigidBodyHandle body) const {
    return m_impl ? m_impl->getBody(body) : nullptr;
}

PhysicsWorld::SoftBodyHandle PhysicsWorld::AddSoftBody(btSoftBody* body) {
    return AddSoftBody(body,
        static_cast<short>(btBroadphaseProxy::DefaultFilter),
        static_cast<short>(btBroadphaseProxy::AllFilter));
}

PhysicsWorld::SoftBodyHandle PhysicsWorld::AddSoftBody(btSoftBody* body, short group, short mask) {
    if (!m_impl || !m_impl->world || !body) return 0;
    m_impl->world->addSoftBody(body, group, mask);
    SoftBodyHandle h = m_impl->nextSoftBodyHandle++;
    m_impl->softBodies[h] = { body, group, mask };
    m_impl->softBodyByPtr[body] = h;
    return h;
}

void PhysicsWorld::RemoveSoftBody(SoftBodyHandle body) {
    if (!m_impl || !m_impl->world) return;
    auto it = m_impl->softBodies.find(body);
    if (it == m_impl->softBodies.end()) return;
    btSoftBody* b = it->second.body;
    m_impl->world->removeSoftBody(b);
    m_impl->softBodyByPtr.erase(b);
    delete b;
    m_impl->softBodies.erase(it);
}

bool PhysicsWorld::HasSoftBody(SoftBodyHandle body) const {
    return m_impl && m_impl->softBodies.find(body) != m_impl->softBodies.end();
}

btSoftBodyWorldInfo& PhysicsWorld::GetSoftBodyWorldInfo() {
    return m_impl->softBodyInfo;
}

btSoftBody* PhysicsWorld::GetSoftBodyInternal(SoftBodyHandle body) const {
    if (!m_impl) return nullptr;
    auto it = m_impl->softBodies.find(body);
    return (it != m_impl->softBodies.end()) ? it->second.body : nullptr;
}

void PhysicsWorld::SetSoftBodyCollisionFilter(SoftBodyHandle body, short group, short mask) {
    if (!m_impl || !m_impl->world) return;
    auto it = m_impl->softBodies.find(body);
    if (it == m_impl->softBodies.end() || !it->second.body) return;
    btSoftBody* b = it->second.body;
    m_impl->world->removeSoftBody(b);
    it->second.group = group;
    it->second.mask = mask;
    m_impl->world->addSoftBody(b, group, mask);
}

} // namespace physics
} // namespace eyelib

