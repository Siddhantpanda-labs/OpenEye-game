#pragma once

#include "../Scene.h"
#include "../Components.h"

// Forward declaration
namespace OpenEye {
namespace Physics {
    class PhysicsWorld;
}
}

namespace OpenEye {
namespace ECS {

/**
 * @brief Physics system - syncs Transform <-> PhysicsBody and steps simulation.
 * 
 * This system:
 * 1. Syncs Transform component -> PhysicsBody (before step)
 * 2. Steps physics simulation
 * 3. Syncs PhysicsBody -> Transform component (after step)
 * 4. Handles cleanup when entities are destroyed
 */
class PhysicsSystem {
public:
    /**
     * @brief Update physics simulation for all entities with PhysicsBody.
     * @param scene ECS scene
     * @param physicsWorld Physics world to step
     * @param deltaTime Time since last frame (in seconds)
     */
    static void Update(Scene& scene, Physics::PhysicsWorld* physicsWorld, float deltaTime);
    
    /**
     * @brief Sync Transform -> PhysicsBody for all entities (before physics step).
     * @param scene ECS scene
     */
    static void SyncTransformToPhysics(Scene& scene);
    
    /**
     * @brief Sync PhysicsBody -> Transform for all entities (after physics step).
     * @param scene ECS scene
     */
    static void SyncPhysicsToTransform(Scene& scene);
    
    /**
     * @brief Create a physics body component for an entity.
     * @param scene ECS scene
     * @param entity Entity to attach physics to
     * @param physicsWorld Physics world
     * @param shape Collision shape (shared_ptr)
     * @param mass Mass (0 = static, > 0 = dynamic)
     * @param isKinematic If true, physics doesn't affect transform
     * @return true if created successfully
     */
    static bool AttachPhysicsBody(
        Scene& scene,
        Entity entity,
        Physics::PhysicsWorld* physicsWorld,
        std::shared_ptr<class btCollisionShape> shape,
        float mass = 0.0f,
        bool isKinematic = false
    );
    
    /**
     * @brief Remove physics body from entity and clean up.
     * @param scene ECS scene
     * @param entity Entity to remove physics from
     * @param physicsWorld Physics world
     */
    static void DetachPhysicsBody(Scene& scene, Entity entity, Physics::PhysicsWorld* physicsWorld);
    
    /**
     * @brief Create a ground plane entity with static physics.
     * @param scene ECS scene
     * @param physicsWorld Physics world
     * @param position Position of ground center
     * @param size Size of ground plane (width, height)
     * @return Created entity
     */
    static Entity CreateGroundPlane(
        Scene& scene,
        Physics::PhysicsWorld* physicsWorld,
        const glm::vec3& position = glm::vec3(0.0f, 0.0f, 0.0f),
        const glm::vec2& size = glm::vec2(100.0f, 100.0f)
    );
};

} // namespace ECS
} // namespace OpenEye

