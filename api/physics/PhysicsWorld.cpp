#include "api/physics/PhysicsWorld.h"

#include "eyelib/physics/PhysicsWorld.h"

namespace eyelib {
namespace api {

class PhysicsWorld::Impl {
public:
    eyelib::physics::PhysicsWorld world{};
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

} // namespace api
} // namespace eyelib

