#pragma once

#include <glm/glm.hpp>
#include <BulletDynamics/Character/btKinematicCharacterController.h>

namespace OpenEye {
namespace Physics {

class PhysicsWorld;

/**
 * @brief Factory for creating and destroying character controllers.
 */
class CharacterControllerFactory {
public:
    // Create character controller
    static btKinematicCharacterController* Create(
        PhysicsWorld* world,
        const glm::vec3& position,
        float radius,
        float height
    );
    
    // Destroy character controller
    static void Destroy(PhysicsWorld* world, btKinematicCharacterController* controller);
    
    // Query if controller exists in world
    static bool ExistsInWorld(PhysicsWorld* world, btKinematicCharacterController* controller);
};

} // namespace Physics
} // namespace OpenEye

