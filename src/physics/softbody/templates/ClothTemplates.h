#pragma once

#include <memory>
#include <glm/glm.hpp>

// Forward declaration
class btSoftBody;

namespace OpenEye {
namespace Physics {
    class PhysicsWorld;
}

namespace SoftBodies {
namespace Templates {

struct ClothConfig {
    glm::vec3 position = glm::vec3(0.0f, 3.0f, 0.0f);
    int widthSegments = 20;
    int heightSegments = 20;
    float width = 4.0f;
    float height = 4.0f;
    float mass = 1.0f;
    
    // Wind settings
    glm::vec3 windDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    float windStrength = 2.0f;
    bool windEnabled = true;
};

// Create cloth with default settings
btSoftBody* CreateCloth(Physics::PhysicsWorld* world, const ClothConfig& config);

// Preset configurations
ClothConfig CreateDefaultClothConfig();
ClothConfig CreateFlagClothConfig(); // Larger, lighter cloth for flag-like behavior
ClothConfig CreateCurtainClothConfig(); // Tall, narrow cloth

} // namespace Templates
} // namespace SoftBodies
} // namespace OpenEye
