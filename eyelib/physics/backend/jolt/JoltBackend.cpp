#include "JoltBackend.h"
#include "JoltWorld.h"
#include "JoltQueries.h"
#include "JoltShapeStore.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/PhysicsSystem.h>

JPH_SUPPRESS_WARNINGS

namespace eyelib {
namespace physics {

namespace {

enum class ObjectLayer : JPH::ObjectLayer {
    NON_MOVING = 0,
    MOVING = 1,
};

static JPH::RVec3 ToJolt(const eyelib::math::Vec3& v) {
    return JPH::RVec3(v.x, v.y, v.z);
}
static JPH::Vec3 ToJoltVec3(const eyelib::math::Vec3& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}
static JPH::Quat ToJolt(const eyelib::math::Quat& q) {
    return JPH::Quat(q.x, q.y, q.z, q.w);
}
static eyelib::math::Vec3 FromJoltVec3(JPH::Vec3Arg v) {
    return eyelib::math::Vec3(v.GetX(), v.GetY(), v.GetZ());
}
static eyelib::math::Vec3 FromJoltRVec3(JPH::RVec3Arg v) {
    return eyelib::math::Vec3(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ()));
}
static eyelib::math::Quat FromJolt(JPH::QuatArg q) {
    return eyelib::math::Quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

} // namespace

struct JoltBackend::Impl {
    JoltWorld world;
    JoltShapeStore shapes;
    JoltQueries queries;
    eyelib::api::physics::ContactCallback contactCb;
};

JoltBackend::JoltBackend() : m_impl(nullptr) {
    JoltWorld::EnsureJoltGlobalInit();
    m_impl = new Impl();
    m_impl->queries.SetWorld(&m_impl->world);
}

JoltBackend::~JoltBackend() {
    Shutdown();
    delete m_impl;
    m_impl = nullptr;
}

bool JoltBackend::Initialize(const eyelib::math::Vec3& gravity) {
    if (!m_impl->world.Init()) return false;
    m_impl->world.SetGravity(gravity);
    if (m_impl->contactCb)
        m_impl->world.SetContactCallback([this](uint32_t a, uint32_t b, const eyelib::math::Vec3& pos, const eyelib::math::Vec3& n) {
            if (m_impl->contactCb) m_impl->contactCb(a, b, pos, n);
        });
    return true;
}

void JoltBackend::Shutdown() {
    m_impl->world.Shutdown();
    m_impl->shapes.Clear();
}

void JoltBackend::Step(float deltaTime, int maxSubSteps, float fixedTimeStep) {
    if (!m_impl->world.IsInitialized()) return;
    int steps = maxSubSteps > 0 ? maxSubSteps : 1;
    float total = deltaTime;
    if (fixedTimeStep > 0.0f) {
        steps = static_cast<int>(total / fixedTimeStep);
        if (steps < 1) steps = 1;
    }
    m_impl->world.Update(deltaTime, steps);
}

void JoltBackend::SetGravity(const eyelib::math::Vec3& gravity) {
    m_impl->world.SetGravity(gravity);
}
eyelib::math::Vec3 JoltBackend::GetGravity() const {
    return m_impl->world.GetGravity();
}

eyelib::api::physics::CollisionShapeHandle JoltBackend::CreateBoxShape(const eyelib::math::Vec3& halfExtents) {
    return m_impl->shapes.CreateBox(halfExtents);
}
eyelib::api::physics::CollisionShapeHandle JoltBackend::CreateSphereShape(float radius) {
    return m_impl->shapes.CreateSphere(radius);
}
eyelib::api::physics::CollisionShapeHandle JoltBackend::CreateCapsuleShape(float radius, float height) {
    return m_impl->shapes.CreateCapsule(radius, height);
}
void JoltBackend::DestroyShape(eyelib::api::physics::CollisionShapeHandle shape) {
    m_impl->shapes.Destroy(shape);
}

eyelib::api::physics::RigidBodyHandle JoltBackend::CreateRigidBody(
    const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation,
    eyelib::api::physics::CollisionShapeHandle shape, float mass,
    const eyelib::math::Vec3& /*inertia*/) {
    if (!m_impl->shapes.IsValid(shape)) return 0;
    const JPH::Shape* jphShape = static_cast<const JPH::Shape*>(m_impl->shapes.GetShapePointer(shape));
    if (!jphShape) return 0;

    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    JPH::EMotionType motionType = (mass > 0.0f) ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
    JPH::ObjectLayer layer = static_cast<JPH::ObjectLayer>(ObjectLayer::MOVING);
    JPH::BodyCreationSettings settings(
        jphShape,
        ToJolt(position),
        ToJolt(rotation),
        motionType,
        layer
    );
    JPH::BodyID id = bi.CreateAndAddBody(settings, JPH::EActivation::Activate);
    if (!id.IsInvalid())
        return id.GetIndexAndSequenceNumber();
    return 0;
}

void JoltBackend::DestroyRigidBody(eyelib::api::physics::RigidBodyHandle body) {
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    JPH::BodyID id(body);
    if (id.IsInvalid()) return;
    bi.RemoveBody(id);
    bi.DestroyBody(id);
}

std::optional<eyelib::api::physics::RigidBodyTransform> JoltBackend::GetRigidBodyTransform(
    eyelib::api::physics::RigidBodyHandle body) const {
    const JPH::BodyInterface* bi = static_cast<const JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    if (!bi) return std::nullopt;
    JPH::BodyID id(body);
    if (id.IsInvalid()) return std::nullopt;
    JPH::RVec3 pos;
    JPH::Quat rot;
    bi->GetPositionAndRotation(id, pos, rot);
    eyelib::api::physics::RigidBodyTransform t;
    t.position = FromJoltRVec3(pos);
    t.rotation = FromJolt(rot);
    return t;
}

bool JoltBackend::SetRigidBodyTransform(eyelib::api::physics::RigidBodyHandle body,
                                       const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation) {
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    JPH::BodyID id(body);
    if (id.IsInvalid()) return false;
    bi.SetPositionAndRotation(id, ToJolt(position), ToJolt(rotation), JPH::EActivation::DontActivate);
    return true;
}

void JoltBackend::SetVelocity(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& velocity) {
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.SetLinearVelocity(JPH::BodyID(body), ToJoltVec3(velocity));
}
eyelib::math::Vec3 JoltBackend::GetVelocity(eyelib::api::physics::RigidBodyHandle body) const {
    const JPH::BodyInterface* bi = static_cast<const JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    return bi ? FromJoltVec3(bi->GetLinearVelocity(JPH::BodyID(body))) : eyelib::math::Vec3(0.0f);
}
void JoltBackend::SetAngularVelocity(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& omega) {
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.SetAngularVelocity(JPH::BodyID(body), ToJoltVec3(omega));
}
eyelib::math::Vec3 JoltBackend::GetAngularVelocity(eyelib::api::physics::RigidBodyHandle body) const {
    const JPH::BodyInterface* bi = static_cast<const JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    return bi ? FromJoltVec3(bi->GetAngularVelocity(JPH::BodyID(body))) : eyelib::math::Vec3(0.0f);
}

void JoltBackend::ApplyForce(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& force,
                             const eyelib::math::Vec3& point) {
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.AddForce(JPH::BodyID(body), ToJoltVec3(force), ToJolt(point));
}
void JoltBackend::ApplyImpulse(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& impulse,
                               const eyelib::math::Vec3& point) {
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.AddImpulse(JPH::BodyID(body), ToJoltVec3(impulse), ToJolt(point));
}

void JoltBackend::SetMass(eyelib::api::physics::RigidBodyHandle body, float mass) {
    (void)body;
    (void)mass;
    // Jolt computes mass from shape density; override would require mass properties. Stub for now.
}
float JoltBackend::GetMass(eyelib::api::physics::RigidBodyHandle body) const {
    (void)body;
    return 1.0f;
}
void JoltBackend::SetFriction(eyelib::api::physics::RigidBodyHandle body, float friction) {
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.SetFriction(JPH::BodyID(body), friction);
}
void JoltBackend::SetRestitution(eyelib::api::physics::RigidBodyHandle body, float restitution) {
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.SetRestitution(JPH::BodyID(body), restitution);
}
void JoltBackend::SetLinearDamping(eyelib::api::physics::RigidBodyHandle body, float /*damping*/) {
    (void)body;
    // Jolt applies damping via MotionProperties; would need body lock. Stub for now.
}
void JoltBackend::SetAngularDamping(eyelib::api::physics::RigidBodyHandle body, float /*damping*/) {
    (void)body;
    // Jolt applies damping via MotionProperties; would need body lock. Stub for now.
}

void JoltBackend::SetKinematic(eyelib::api::physics::RigidBodyHandle body, bool kinematic) {
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.SetMotionType(JPH::BodyID(body), kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
}
bool JoltBackend::IsKinematic(eyelib::api::physics::RigidBodyHandle body) const {
    const JPH::BodyInterface* bi = static_cast<const JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    return bi && bi->GetMotionType(JPH::BodyID(body)) == JPH::EMotionType::Kinematic;
}
void JoltBackend::SetRigidBodyGravity(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& gravity) {
    (void)body;
    (void)gravity;
    // Per-body gravity in Jolt is a factor; would use SetGravityFactor. Stub.
}
void JoltBackend::SetCollisionFilter(eyelib::api::physics::RigidBodyHandle body, short /*group*/, short /*mask*/) {
    (void)body;
}

void JoltBackend::SetContactCallback(eyelib::api::physics::ContactCallback cb) {
    m_impl->contactCb = std::move(cb);
    m_impl->world.SetContactCallback([this](uint32_t a, uint32_t b, const eyelib::math::Vec3& pos, const eyelib::math::Vec3& n) {
        if (m_impl->contactCb) m_impl->contactCb(a, b, pos, n);
    });
}

size_t JoltBackend::GetBodyCount() const {
    const JPH::PhysicsSystem* ps = static_cast<const JPH::PhysicsSystem*>(m_impl->world.GetPhysicsSystem());
    return ps ? ps->GetNumBodies() : 0;
}
size_t JoltBackend::GetConstraintCount() const { return 0; }

eyelib::api::physics::RaycastResult JoltBackend::Raycast(const eyelib::math::Vec3& from, const eyelib::math::Vec3& to) const {
    JoltRaycastResult r = m_impl->queries.Raycast(from, to);
    eyelib::api::physics::RaycastResult out;
    out.hit = r.hit;
    out.point = r.point;
    out.normal = r.normal;
    out.distance = r.distance;
    out.body = r.bodyHandle;
    return out;
}

std::vector<eyelib::api::physics::RigidBodyHandle> JoltBackend::QueryAABB(const eyelib::math::AABB& aabb) const {
    return m_impl->queries.QueryAABB(aabb);
}

} // namespace physics
} // namespace eyelib
