#include "ClothTemplates.h"
#include "physics/softbody/SoftBodyFactory.h"
#include "core/log/Log.h"
#include <BulletSoftBody/btSoftBody.h>

using namespace OpenEye::Core;

namespace OpenEye {
namespace SoftBodies {
namespace Templates {

btSoftBody* CreateCloth(Physics::PhysicsWorld* world, const ClothConfig& config) {
    return Physics::SoftBodyFactory::CreateCloth(
        world,
        config.position,
        config.widthSegments,
        config.heightSegments,
        config.width,
        config.height,
        config.mass
    );
}

ClothConfig CreateDefaultClothConfig() {
    ClothConfig config;
    config.position = glm::vec3(0.0f, 3.0f, 0.0f);
    config.widthSegments = 20;
    config.heightSegments = 20;
    config.width = 4.0f;
    config.height = 4.0f;
    config.mass = 1.0f;
    config.windDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    config.windStrength = 2.0f;
    config.windEnabled = true;
    return config;
}

ClothConfig CreateFlagClothConfig() {
    ClothConfig config;
    config.position = glm::vec3(0.0f, 4.0f, 0.0f);
    config.widthSegments = 30;
    config.heightSegments = 20;
    config.width = 6.0f;
    config.height = 4.0f;
    config.mass = 0.5f; // Lighter for more flag-like movement
    config.windDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    config.windStrength = 3.0f; // Stronger wind
    config.windEnabled = true;
    return config;
}

ClothConfig CreateCurtainClothConfig() {
    ClothConfig config;
    config.position = glm::vec3(0.0f, 3.0f, 0.0f);
    config.widthSegments = 15;
    config.heightSegments = 30;
    config.width = 2.0f;
    config.height = 6.0f;
    config.mass = 0.8f;
    config.windDirection = glm::vec3(0.5f, 0.0f, 0.0f);
    config.windStrength = 1.5f;
    config.windEnabled = true;
    return config;
}

} // namespace Templates
} // namespace SoftBodies
} // namespace OpenEye
