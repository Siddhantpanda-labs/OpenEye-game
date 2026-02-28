#include "api/physics/PhysicsWorld.h"
#include "api/physics/IPhysicsWorld.h"
#include "api/physics/PhysicsBackendFactory.h"

#include <memory>

namespace eyelib {
namespace api {
namespace physics {

class PhysicsWorld::Impl {
public:
    std::unique_ptr<IPhysicsWorld> backend;
    std::vector<DebugLine> debugLines;
};

PhysicsWorld::PhysicsWorld() : PhysicsWorld(PhysicsBackendType::Jolt) {}

PhysicsWorld::PhysicsWorld(PhysicsBackendType backendType) : m_impl(new Impl()) {
    m_impl->backend = CreatePhysicsBackend(static_cast<eyelib::api::physics::PhysicsBackendType>(backendType));
}

PhysicsWorld::~PhysicsWorld() {
    Shutdown();
    delete m_impl;
    m_impl = nullptr;
}

bool PhysicsWorld::Initialize(const eyelib::math::Vec3& gravity) {
    return m_impl->backend && m_impl->backend->Initialize(gravity);
}

void PhysicsWorld::Shutdown() {
    if (!m_impl || !m_impl->backend) return;
    m_impl->backend->Shutdown();
}

void PhysicsWorld::Step(float deltaTime, int maxSubSteps, float fixedTimeStep) {
    if (m_impl->backend) m_impl->backend->Step(deltaTime, maxSubSteps, fixedTimeStep);
}

CollisionShapeHandle PhysicsWorld::CreateBoxShape(const eyelib::math::Vec3& halfExtents) {
    return m_impl->backend ? m_impl->backend->CreateBoxShape(halfExtents) : 0;
}

CollisionShapeHandle PhysicsWorld::CreateSphereShape(float radius) {
    return m_impl->backend ? m_impl->backend->CreateSphereShape(radius) : 0;
}

CollisionShapeHandle PhysicsWorld::CreateCapsuleShape(float radius, float height) {
    return m_impl->backend ? m_impl->backend->CreateCapsuleShape(radius, height) : 0;
}

void PhysicsWorld::DestroyShape(CollisionShapeHandle shape) {
    if (m_impl->backend) m_impl->backend->DestroyShape(shape);
}

RigidBodyHandle PhysicsWorld::CreateBoxRigidBody(float mass, const eyelib::math::Vec3& halfExtents, const eyelib::math::Vec3& position) {
    if (!m_impl->backend) return 0;
    CollisionShapeHandle shape = m_impl->backend->CreateBoxShape(halfExtents);
    if (shape == 0) return 0;
    RigidBodyHandle body = m_impl->backend->CreateRigidBody(position, eyelib::math::Quat(1.f, 0.f, 0.f, 0.f), shape, mass);
    if (body == 0) m_impl->backend->DestroyShape(shape);
    return body;
}

RigidBodyHandle PhysicsWorld::CreateRigidBody(const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation,
                                              CollisionShapeHandle shape, float mass,
                                              const eyelib::math::Vec3& inertia) {
    return m_impl->backend ? m_impl->backend->CreateRigidBody(position, rotation, shape, mass, inertia) : 0;
}

void PhysicsWorld::DestroyRigidBody(RigidBodyHandle body) {
    if (m_impl->backend) m_impl->backend->DestroyRigidBody(body);
}

std::optional<RigidBodyTransform> PhysicsWorld::GetRigidBodyTransform(RigidBodyHandle body) const {
    return m_impl->backend ? m_impl->backend->GetRigidBodyTransform(body) : std::nullopt;
}

bool PhysicsWorld::SetRigidBodyTransform(RigidBodyHandle body, const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation) {
    return m_impl->backend && m_impl->backend->SetRigidBodyTransform(body, position, rotation);
}

void PhysicsWorld::SetGravity(const eyelib::math::Vec3& gravity) {
    if (m_impl->backend) m_impl->backend->SetGravity(gravity);
}

eyelib::math::Vec3 PhysicsWorld::GetGravity() const {
    return m_impl->backend ? m_impl->backend->GetGravity() : eyelib::math::Vec3(0.f, -9.81f, 0.f);
}

void PhysicsWorld::SetSolverIterations(int /*iterations*/) {}

void PhysicsWorld::SetSolverSubSteps(int /*subSteps*/) {}

void PhysicsWorld::SetCollisionFilter(RigidBodyHandle body, short group, short mask) {
    if (m_impl->backend) m_impl->backend->SetCollisionFilter(body, group, mask);
}

size_t PhysicsWorld::GetBodyCount() const {
    return m_impl->backend ? m_impl->backend->GetBodyCount() : 0;
}

size_t PhysicsWorld::GetConstraintCount() const {
    return m_impl->backend ? m_impl->backend->GetConstraintCount() : 0;
}

void PhysicsWorld::SetContactCallback(std::function<void(RigidBodyHandle, RigidBodyHandle, const eyelib::math::Vec3&, const eyelib::math::Vec3&)> cb) {
    if (m_impl->backend) m_impl->backend->SetContactCallback(std::move(cb));
}

void PhysicsWorld::SetContactCallbackEx(ContactCallbackEx /*cb*/) {}

void PhysicsWorld::CollectDebugDrawData() {
    m_impl->debugLines.clear();
}

const std::vector<DebugLine>& PhysicsWorld::GetDebugDrawData() const {
    return m_impl->debugLines;
}

IPhysicsWorld* PhysicsWorld::GetInternal() {
    return m_impl && m_impl->backend ? m_impl->backend.get() : nullptr;
}

const IPhysicsWorld* PhysicsWorld::GetInternal() const {
    return m_impl && m_impl->backend ? m_impl->backend.get() : nullptr;
}

} // namespace physics
} // namespace api
} // namespace eyelib
