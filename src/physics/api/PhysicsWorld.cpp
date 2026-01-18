#include "PhysicsWorld.h"
#include "../PhysicsWorld.h"

namespace OpenEye {
namespace Physics {
namespace API {

// Implementation class that wraps the actual PhysicsWorld
class PhysicsWorld::PhysicsWorldImpl {
public:
    ::OpenEye::Physics::PhysicsWorld world;
};

PhysicsWorld::PhysicsWorld() : m_impl(std::make_unique<PhysicsWorldImpl>()) {}
PhysicsWorld::~PhysicsWorld() = default;

PhysicsWorld::PhysicsWorld(PhysicsWorld&&) noexcept = default;
PhysicsWorld& PhysicsWorld::operator=(PhysicsWorld&&) noexcept = default;

void PhysicsWorld::Initialize(const glm::vec3& gravity) {
    m_impl->world.Initialize(gravity);
}

void PhysicsWorld::Shutdown() {
    m_impl->world.Shutdown();
}

void PhysicsWorld::Step(float deltaTime, int maxSubSteps) {
    m_impl->world.Step(deltaTime, maxSubSteps);
}

void* PhysicsWorld::GetInternalWorld() const {
    return &m_impl->world;
}

} // namespace API
} // namespace Physics
} // namespace OpenEye

