#pragma once

#include <glm/glm.hpp>

// Forward declaration
class btSoftBody;

namespace OpenEye {
namespace Physics { class PhysicsWorld; }

namespace SoftBodies {
namespace Templates {

struct VolumeSphereConfig {
    glm::vec3 position = glm::vec3(0.0f, 8.0f, 0.0f);
    float radius = 0.75f;
    int resolution = 512;
    float mass = 2.0f;
    float pressure = 150.0f; // higher = more inflated / stiffer
};

btSoftBody* CreateVolumeSphere(Physics::PhysicsWorld* world, const VolumeSphereConfig& cfg);
VolumeSphereConfig CreateDefaultVolumeSphereConfig();

} // namespace Templates
} // namespace SoftBodies
} // namespace OpenEye

