#include "eyelib/physics/PhysicsWorld.h"

#include "eyelib/core/Log.h"

#include <unordered_map>

#include <btBulletDynamicsCommon.h>

namespace eyelib {
namespace physics {

namespace {
btVector3 ToBt(const math::Vec3& v) { return btVector3(v.x, v.y, v.z); }
math::Vec3 ToGlm(const btVector3& v) { return math::Vec3(v.x(), v.y(), v.z()); }

btQuaternion ToBt(const math::Quat& q) { return btQuaternion(q.x, q.y, q.z, q.w); }
math::Quat ToGlm(const btQuaternion& q) { return math::Quat(q.w(), q.x(), q.y(), q.z()); }
} // namespace

class PhysicsWorld::Impl {
public:
    btDefaultCollisionConfiguration* collisionConfig = nullptr;
    btCollisionDispatcher* dispatcher = nullptr;
    btBroadphaseInterface* broadphase = nullptr;
    btSequentialImpulseConstraintSolver* solver = nullptr;
    btDiscreteDynamicsWorld* world = nullptr;

    struct BodyEntry {
        btCollisionShape* shape = nullptr;
        btDefaultMotionState* motionState = nullptr;
        btRigidBody* body = nullptr;
    };

    RigidBodyHandle nextHandle = 1;
    std::unordered_map<RigidBodyHandle, BodyEntry> bodies;

    void DestroyAllBodies() {
        if (!world) return;
        for (auto& [h, e] : bodies) {
            (void)h;
            if (e.body) {
                world->removeRigidBody(e.body);
            }
            delete e.body;
            delete e.motionState;
            delete e.shape;
        }
        bodies.clear();
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

    m_impl->collisionConfig = new btDefaultCollisionConfiguration();
    m_impl->dispatcher = new btCollisionDispatcher(m_impl->collisionConfig);
    m_impl->broadphase = new btDbvtBroadphase();
    m_impl->solver = new btSequentialImpulseConstraintSolver();
    m_impl->world = new btDiscreteDynamicsWorld(
        m_impl->dispatcher,
        m_impl->broadphase,
        m_impl->solver,
        m_impl->collisionConfig
    );

    m_impl->world->setGravity(ToBt(gravity));
    core::Log::Info("PhysicsWorld initialized");
    return true;
}

void PhysicsWorld::Shutdown() {
    if (!m_impl) return;
    if (!m_impl->world) return;

    m_impl->DestroyAllBodies();

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
    m_impl->world->stepSimulation(clamped, maxSubSteps, fixedTimeStep);
}

RigidBodyHandle PhysicsWorld::CreateBoxRigidBody(float mass, const math::Vec3& halfExtents, const math::Vec3& position) {
    if (!m_impl || !m_impl->world) return 0;

    btCollisionShape* shape = new btBoxShape(ToBt(halfExtents));

    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(ToBt(position));

    btVector3 inertia(0, 0, 0);
    if (mass > 0.0f) {
        shape->calculateLocalInertia(mass, inertia);
    }

    btDefaultMotionState* motionState = new btDefaultMotionState(startTransform);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, inertia);
    btRigidBody* body = new btRigidBody(rbInfo);

    m_impl->world->addRigidBody(body);

    RigidBodyHandle handle = m_impl->nextHandle++;
    m_impl->bodies.emplace(handle, Impl::BodyEntry{ shape, motionState, body });
    return handle;
}

void PhysicsWorld::DestroyRigidBody(RigidBodyHandle body) {
    if (!m_impl || !m_impl->world) return;
    auto it = m_impl->bodies.find(body);
    if (it == m_impl->bodies.end()) return;

    auto& e = it->second;
    if (e.body) m_impl->world->removeRigidBody(e.body);
    delete e.body;
    delete e.motionState;
    delete e.shape;
    m_impl->bodies.erase(it);
}

std::optional<RigidBodyTransform> PhysicsWorld::GetRigidBodyTransform(RigidBodyHandle body) const {
    if (!m_impl || !m_impl->world) return std::nullopt;
    auto it = m_impl->bodies.find(body);
    if (it == m_impl->bodies.end() || !it->second.body) return std::nullopt;

    btTransform t;
    it->second.body->getMotionState()->getWorldTransform(t);

    RigidBodyTransform out;
    out.position = ToGlm(t.getOrigin());
    out.rotation = ToGlm(t.getRotation());
    return out;
}

bool PhysicsWorld::SetRigidBodyTransform(RigidBodyHandle body, const math::Vec3& position, const math::Quat& rotation) {
    if (!m_impl || !m_impl->world) return false;
    auto it = m_impl->bodies.find(body);
    if (it == m_impl->bodies.end() || !it->second.body) return false;

    btTransform t;
    t.setIdentity();
    t.setOrigin(ToBt(position));
    t.setRotation(ToBt(rotation));

    it->second.body->setWorldTransform(t);
    if (it->second.body->getMotionState()) {
        it->second.body->getMotionState()->setWorldTransform(t);
    }
    it->second.body->activate(true);
    return true;
}

void PhysicsWorld::SetGravity(const math::Vec3& gravity) {
    if (!m_impl || !m_impl->world) return;
    m_impl->world->setGravity(ToBt(gravity));
}

math::Vec3 PhysicsWorld::GetGravity() const {
    if (!m_impl || !m_impl->world) return math::Vec3(0.0f, -9.81f, 0.0f);
    return ToGlm(m_impl->world->getGravity());
}

} // namespace physics
} // namespace eyelib

