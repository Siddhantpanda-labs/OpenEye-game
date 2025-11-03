#pragma once

#include "PhysicsQueryTypes.h"
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Forward declarations
namespace OpenEye {
namespace Physics {
    class PhysicsWorld;
}

namespace Rendering { class Mesh; }
}

// Bullet forward declarations
class btCollisionShape;

namespace OpenEye {
namespace Physics {

/**
 * @brief Physics query system - raycasting, sweep tests, etc.
 */
class PhysicsQuery {
public:
    // Raycast from point to point (returns all hits)
    static RaycastResult Raycast(PhysicsWorld* world, const glm::vec3& from, const glm::vec3& to);
    
    // Raycast closest hit
    static RaycastResult RaycastClosest(PhysicsWorld* world, const glm::vec3& from, const glm::vec3& to);
    
    // Sweep test (continuous collision detection)
    static SweepResult SweepTest(
        PhysicsWorld* world,
        std::shared_ptr<btCollisionShape> shape,
        const glm::vec3& from,
        const glm::vec3& to,
        const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
    );
};

} // namespace Physics
} // namespace OpenEye

