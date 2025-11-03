#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <btBulletDynamicsCommon.h>

namespace OpenEye {
namespace Physics {

class PhysicsWorld;

/**
 * @brief Factory for creating and destroying rigid bodies.
 */
class RigidBodyFactory {
public:
    // Create rigid body
    static btRigidBody* Create(
        PhysicsWorld* world,
        const glm::vec3& position,
        const glm::quat& rotation,
        std::shared_ptr<btCollisionShape> shape,
        float mass = 0.0f,
        const glm::vec3& inertia = glm::vec3(0.0f)
    );
    
    // Destroy rigid body
    static void Destroy(PhysicsWorld* world, btRigidBody* body);
    
    // Query if body exists in world
    static bool ExistsInWorld(PhysicsWorld* world, btRigidBody* body);
};

} // namespace Physics
} // namespace OpenEye

