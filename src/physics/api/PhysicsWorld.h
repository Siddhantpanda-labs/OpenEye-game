#pragma once

#include <memory>
#include <glm/glm.hpp>

// Forward declarations - no Bullet types exposed
namespace OpenEye {
namespace Rendering { class Mesh; }
}

namespace OpenEye {
namespace Physics {
namespace API {

/**
 * @brief Physics World API - Clean interface without exposing Bullet
 * 
 * This is the public API that apps should use.
 * It hides all Bullet dependencies and provides a clean interface.
 */
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();
    
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;
    PhysicsWorld(PhysicsWorld&&) noexcept;
    PhysicsWorld& operator=(PhysicsWorld&&) noexcept;
    
    // Initialize/Shutdown
    void Initialize(const glm::vec3& gravity = glm::vec3(0.0f, -9.81f, 0.0f));
    void Shutdown();
    
    // Simulation stepping
    void Step(float deltaTime, int maxSubSteps = 3);
    
    // Get internal implementation (for advanced use with ECS)
    void* GetInternalWorld() const;
    
private:
    class PhysicsWorldImpl;
    std::unique_ptr<PhysicsWorldImpl> m_impl;
};

} // namespace API
} // namespace Physics
} // namespace OpenEye

