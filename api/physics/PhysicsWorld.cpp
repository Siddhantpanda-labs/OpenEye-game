#include "api/physics/PhysicsWorld.h"

#include "eyelib/physics/PhysicsWorld.h"

namespace eyelib {
namespace api {
namespace physics {

class PhysicsWorld::Impl {
public:
    eyelib::physics::PhysicsWorld world{};
    std::vector<DebugLine> debugLines;
};

PhysicsWorld::PhysicsWorld() : m_impl(new Impl()) {}

PhysicsWorld::~PhysicsWorld() {
    Shutdown();
    delete m_impl;
    m_impl = nullptr;
}

bool PhysicsWorld::Initialize(const eyelib::math::Vec3& gravity) {
    return m_impl->world.Initialize(gravity);
}

void PhysicsWorld::Shutdown() {
    if (!m_impl) return;
    m_impl->world.Shutdown();
}

void PhysicsWorld::Step(float deltaTime, int maxSubSteps, float fixedTimeStep) {
    m_impl->world.Step(deltaTime, maxSubSteps, fixedTimeStep);
}

RigidBodyHandle PhysicsWorld::CreateBoxRigidBody(float mass, const eyelib::math::Vec3& halfExtents, const eyelib::math::Vec3& position) {
    return m_impl->world.CreateBoxRigidBody(mass, halfExtents, position);
}

void PhysicsWorld::DestroyRigidBody(RigidBodyHandle body) {
    m_impl->world.DestroyRigidBody(body);
}

std::optional<RigidBodyTransform> PhysicsWorld::GetRigidBodyTransform(RigidBodyHandle body) const {
    auto t = m_impl->world.GetRigidBodyTransform(body);
    if (!t) return std::nullopt;
    return RigidBodyTransform{ t->position, t->rotation };
}

bool PhysicsWorld::SetRigidBodyTransform(RigidBodyHandle body, const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation) {
    return m_impl->world.SetRigidBodyTransform(body, position, rotation);
}

void PhysicsWorld::SetGravity(const eyelib::math::Vec3& gravity) {
    m_impl->world.SetGravity(gravity);
}

eyelib::math::Vec3 PhysicsWorld::GetGravity() const {
    return m_impl->world.GetGravity();
}

void PhysicsWorld::SetSolverIterations(int iterations) {
    m_impl->world.SetSolverIterations(iterations);
}

void PhysicsWorld::SetSolverSubSteps(int subSteps) {
    m_impl->world.SetSolverSubSteps(subSteps);
}

void PhysicsWorld::SetCollisionFilter(RigidBodyHandle body, short group, short mask) {
    m_impl->world.SetCollisionFilter(body, group, mask);
}

size_t PhysicsWorld::GetBodyCount() const {
    return m_impl->world.GetBodyCount();
}

size_t PhysicsWorld::GetConstraintCount() const {
    return m_impl->world.GetConstraintCount();
}

void PhysicsWorld::SetContactCallback(std::function<void(RigidBodyHandle, RigidBodyHandle, const eyelib::math::Vec3&, const eyelib::math::Vec3&)> cb) {
    m_impl->world.SetContactCallback(std::move(cb));
}

void PhysicsWorld::SetContactCallbackEx(ContactCallbackEx cb) {
    auto* w = GetInternal();
    if (!w) return;
    w->SetContactCallbackEx([cb](const ::eyelib::physics::PhysicsWorld::ContactObject& a,
                                 const ::eyelib::physics::PhysicsWorld::ContactObject& b,
                                 const eyelib::math::Vec3& point, const eyelib::math::Vec3& normal) {
        if (!cb) return;
        ContactObject oa{};
        ContactObject ob{};
        oa.type = static_cast<ContactObjectType>(a.type);
        ob.type = static_cast<ContactObjectType>(b.type);
        oa.handle = a.handle;
        ob.handle = b.handle;
        cb(oa, ob, point, normal);
    });
}

void PhysicsWorld::CollectDebugDrawData() {
    m_impl->world.CollectDebugDrawData();
    const auto& lines = m_impl->world.GetDebugDrawData();
    m_impl->debugLines.clear();
    m_impl->debugLines.reserve(lines.size());
    for (const auto& l : lines) {
        DebugLine d;
        d.from = l.from;
        d.to = l.to;
        d.color = l.color;
        m_impl->debugLines.push_back(d);
    }
}

const std::vector<DebugLine>& PhysicsWorld::GetDebugDrawData() const {
    return m_impl->debugLines;
}

::eyelib::physics::PhysicsWorld* PhysicsWorld::GetInternal() {
    return m_impl ? &m_impl->world : nullptr;
}

const ::eyelib::physics::PhysicsWorld* PhysicsWorld::GetInternal() const {
    return m_impl ? &m_impl->world : nullptr;
}

} // namespace physics
} // namespace api
} // namespace eyelib

