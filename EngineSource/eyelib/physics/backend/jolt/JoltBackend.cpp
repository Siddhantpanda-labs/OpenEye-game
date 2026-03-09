#include "JoltBackend.h"
#include "JoltWorld.h"
#include "JoltQueries.h"
#include "JoltShapeStore.h"
#include "core/Log.h"

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

static JPH::BodyID ToBodyID(eyelib::api::physics::RigidBodyHandle h) {
    return JPH::BodyID(h.Raw());
}

static bool IsValidBody(eyelib::api::physics::RigidBodyHandle h) {
    if (!h) return false;
    JPH::BodyID id(h.Raw());
    return !id.IsInvalid();
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
    if (m_impl) {
        core::Log::WarnF("[JoltBackend] destructor forced cleanup (Shutdown() was not called)");
        delete m_impl;
        m_impl = nullptr;
    }
}

bool JoltBackend::Initialize(const eyelib::math::Vec3& gravity) {
    if (!m_impl->world.Init()) return false;
    m_impl->world.SetGravity(gravity);
    if (m_impl->contactCb)
        m_impl->world.SetContactCallback([this](uint32_t a, uint32_t b, const eyelib::math::Vec3& pos, const eyelib::math::Vec3& n) {
            if (m_impl->contactCb) {
                m_impl->contactCb(
                    eyelib::api::physics::RigidBodyHandle::FromRaw(a),
                    eyelib::api::physics::RigidBodyHandle::FromRaw(b),
                    pos, n);
            }
        });
    return true;
}

void JoltBackend::Shutdown() {
    if (!m_impl) return;
    core::Log::InfoF("[JoltBackend] Shutdown (per Play session)");
    m_impl->world.Shutdown();
    m_impl->shapes.Clear();
    delete m_impl;
    m_impl = nullptr;
}

void JoltBackend::Step(float deltaTime, int maxSubSteps, float fixedTimeStep) {
    if (!m_impl->world.IsInitialized()) return;
    int steps = maxSubSteps > 0 ? maxSubSteps : 1;
    if (fixedTimeStep > 0.0f) {
        steps = static_cast<int>(deltaTime / fixedTimeStep);
        if (steps < 1) steps = 1;
        if (steps > 8) steps = 8;
    }
    m_impl->world.Update(deltaTime, steps);
}

void JoltBackend::SetGravity(const eyelib::math::Vec3& gravity) {
    m_impl->world.SetGravity(gravity);
}
eyelib::math::Vec3 JoltBackend::GetGravity() const {
    return m_impl->world.GetGravity();
}

// --- Shapes ---

eyelib::api::physics::CollisionShapeHandle JoltBackend::CreateBoxShape(const eyelib::math::Vec3& halfExtents) {
    uint32_t raw = m_impl->shapes.CreateBox(halfExtents);
    return eyelib::api::physics::CollisionShapeHandle::FromRaw(raw);
}
eyelib::api::physics::CollisionShapeHandle JoltBackend::CreateSphereShape(float radius) {
    uint32_t raw = m_impl->shapes.CreateSphere(radius);
    return eyelib::api::physics::CollisionShapeHandle::FromRaw(raw);
}
eyelib::api::physics::CollisionShapeHandle JoltBackend::CreateCapsuleShape(float radius, float height) {
    uint32_t raw = m_impl->shapes.CreateCapsule(radius, height);
    return eyelib::api::physics::CollisionShapeHandle::FromRaw(raw);
}
void JoltBackend::DestroyShape(eyelib::api::physics::CollisionShapeHandle shape) {
    if (!shape) return;
    m_impl->shapes.Destroy(shape.Raw());
}

// --- Rigid body creation/destruction ---

eyelib::api::physics::RigidBodyHandle JoltBackend::CreateRigidBody(
    const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation,
    eyelib::api::physics::CollisionShapeHandle shape, float mass,
    const eyelib::math::Vec3& /*inertia*/) {
    if (!shape || !m_impl->shapes.IsValid(shape.Raw())) return {};
    const JPH::Shape* jphShape = static_cast<const JPH::Shape*>(m_impl->shapes.GetShapePointer(shape.Raw()));
    if (!jphShape) return {};

    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    const bool isDynamic = mass > 0.0f;
    JPH::EMotionType motionType = isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
    JPH::ObjectLayer layer = isDynamic
        ? static_cast<JPH::ObjectLayer>(ObjectLayer::MOVING)
        : static_cast<JPH::ObjectLayer>(ObjectLayer::NON_MOVING);
    JPH::BodyCreationSettings settings(
        jphShape,
        ToJolt(position),
        ToJolt(rotation),
        motionType,
        layer
    );
    JPH::EActivation activation = isDynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
    JPH::BodyID id = bi.CreateAndAddBody(settings, activation);
    if (id.IsInvalid())
        return {};
    uint32_t raw = id.GetIndexAndSequenceNumber();
    if (raw == 0 || raw == 0xFFFFFFFFu)
        return {};
    return eyelib::api::physics::RigidBodyHandle::FromRaw(raw);
}

void JoltBackend::DestroyRigidBody(eyelib::api::physics::RigidBodyHandle body) {
    if (!IsValidBody(body)) return;
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    JPH::BodyID id = ToBodyID(body);
    bi.RemoveBody(id);
    bi.DestroyBody(id);
}

// --- Transform ---

std::optional<eyelib::api::physics::RigidBodyTransform> JoltBackend::GetRigidBodyTransform(
    eyelib::api::physics::RigidBodyHandle body) const {
    if (!IsValidBody(body)) return std::nullopt;
    const JPH::BodyInterface* bi = static_cast<const JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    if (!bi) return std::nullopt;
    JPH::RVec3 pos;
    JPH::Quat rot;
    bi->GetPositionAndRotation(ToBodyID(body), pos, rot);
    eyelib::api::physics::RigidBodyTransform t;
    t.position = FromJoltRVec3(pos);
    t.rotation = FromJolt(rot);
    return t;
}

bool JoltBackend::SetRigidBodyTransform(eyelib::api::physics::RigidBodyHandle body,
                                       const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation) {
    if (!IsValidBody(body)) return false;
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.SetPositionAndRotation(ToBodyID(body), ToJolt(position), ToJolt(rotation), JPH::EActivation::DontActivate);
    return true;
}

// --- Velocity ---

void JoltBackend::SetVelocity(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& velocity) {
    if (!IsValidBody(body)) return;
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.SetLinearVelocity(ToBodyID(body), ToJoltVec3(velocity));
}
eyelib::math::Vec3 JoltBackend::GetVelocity(eyelib::api::physics::RigidBodyHandle body) const {
    if (!IsValidBody(body)) return eyelib::math::Vec3(0.0f);
    const JPH::BodyInterface* bi = static_cast<const JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    return bi ? FromJoltVec3(bi->GetLinearVelocity(ToBodyID(body))) : eyelib::math::Vec3(0.0f);
}
void JoltBackend::SetAngularVelocity(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& omega) {
    if (!IsValidBody(body)) return;
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.SetAngularVelocity(ToBodyID(body), ToJoltVec3(omega));
}
eyelib::math::Vec3 JoltBackend::GetAngularVelocity(eyelib::api::physics::RigidBodyHandle body) const {
    if (!IsValidBody(body)) return eyelib::math::Vec3(0.0f);
    const JPH::BodyInterface* bi = static_cast<const JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    return bi ? FromJoltVec3(bi->GetAngularVelocity(ToBodyID(body))) : eyelib::math::Vec3(0.0f);
}

// --- Forces ---

void JoltBackend::ApplyForce(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& force,
                             const eyelib::math::Vec3& point) {
    if (!IsValidBody(body)) return;
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.AddForce(ToBodyID(body), ToJoltVec3(force), ToJolt(point));
}
void JoltBackend::ApplyImpulse(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& impulse,
                               const eyelib::math::Vec3& point) {
    if (!IsValidBody(body)) return;
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.AddImpulse(ToBodyID(body), ToJoltVec3(impulse), ToJolt(point));
}

// --- Properties ---

void JoltBackend::SetMass(eyelib::api::physics::RigidBodyHandle body, float mass) {
    (void)body; (void)mass;
}
float JoltBackend::GetMass(eyelib::api::physics::RigidBodyHandle body) const {
    (void)body;
    return 1.0f;
}
void JoltBackend::SetFriction(eyelib::api::physics::RigidBodyHandle body, float friction) {
    if (!IsValidBody(body)) return;
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.SetFriction(ToBodyID(body), friction);
}
void JoltBackend::SetRestitution(eyelib::api::physics::RigidBodyHandle body, float restitution) {
    if (!IsValidBody(body)) return;
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.SetRestitution(ToBodyID(body), restitution);
}
void JoltBackend::SetLinearDamping(eyelib::api::physics::RigidBodyHandle body, float /*damping*/) {
    (void)body;
}
void JoltBackend::SetAngularDamping(eyelib::api::physics::RigidBodyHandle body, float /*damping*/) {
    (void)body;
}

void JoltBackend::SetKinematic(eyelib::api::physics::RigidBodyHandle body, bool kinematic) {
    if (!IsValidBody(body)) return;
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    bi.SetMotionType(ToBodyID(body), kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
}
bool JoltBackend::IsKinematic(eyelib::api::physics::RigidBodyHandle body) const {
    if (!IsValidBody(body)) return false;
    const JPH::BodyInterface* bi = static_cast<const JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    return bi && bi->GetMotionType(ToBodyID(body)) == JPH::EMotionType::Kinematic;
}
void JoltBackend::SetRigidBodyGravity(eyelib::api::physics::RigidBodyHandle body, const eyelib::math::Vec3& gravity) {
    if (!IsValidBody(body)) return;
    JPH::BodyInterface& bi = *static_cast<JPH::BodyInterface*>(m_impl->world.GetBodyInterface());
    float factor = (gravity.x == 0.0f && gravity.y == 0.0f && gravity.z == 0.0f) ? 0.0f : 1.0f;
    bi.SetGravityFactor(ToBodyID(body), factor);
}
void JoltBackend::SetCollisionFilter(eyelib::api::physics::RigidBodyHandle body, short /*group*/, short /*mask*/) {
    (void)body;
}

void JoltBackend::SetContactCallback(eyelib::api::physics::ContactCallback cb) {
    m_impl->contactCb = std::move(cb);
    m_impl->world.SetContactCallback([this](uint32_t a, uint32_t b, const eyelib::math::Vec3& pos, const eyelib::math::Vec3& n) {
        if (m_impl->contactCb) {
            m_impl->contactCb(
                eyelib::api::physics::RigidBodyHandle::FromRaw(a),
                eyelib::api::physics::RigidBodyHandle::FromRaw(b),
                pos, n);
        }
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
    out.body = eyelib::api::physics::RigidBodyHandle::FromRaw(r.bodyHandle);
    return out;
}

std::vector<eyelib::api::physics::RigidBodyHandle> JoltBackend::QueryAABB(const eyelib::math::AABB& aabb) const {
    auto raw = m_impl->queries.QueryAABB(aabb);
    std::vector<eyelib::api::physics::RigidBodyHandle> out;
    out.reserve(raw.size());
    for (uint32_t r : raw) {
        out.push_back(eyelib::api::physics::RigidBodyHandle::FromRaw(r));
    }
    return out;
}

} // namespace physics
} // namespace eyelib
