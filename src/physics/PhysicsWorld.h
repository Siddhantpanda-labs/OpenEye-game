#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftBody.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>

// Forward declaration for character controller (not in btBulletDynamicsCommon.h)
class btKinematicCharacterController;
class btSoftBodySolver;

// Forward declarations
namespace OpenEye {
namespace Rendering { class Mesh; }
namespace Physics {
    namespace Utils { class PhysicsUtils; }
    class CollisionShapeFactory;
    class RigidBodyFactory;
    class CharacterControllerFactory;
}
}

#include "queries/PhysicsQueryTypes.h"

namespace OpenEye {
namespace Physics {

/**
 * @brief Core physics simulation world manager.
 * 
 * Manages the Bullet physics world, handles simulation stepping,
 * and delegates to factories for object creation.
 * 
 * Architecture: Central orchestrator for physics (similar to RenderPipeline).
 */
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    // Initialize/Shutdown
    void Initialize(const glm::vec3& gravity = glm::vec3(0.0f, -9.81f, 0.0f));
    void Shutdown();

    // Simulation stepping
    void Step(float deltaTime, int maxSubSteps = 3);
    
    // Factory method delegates (forward to factories)
    std::shared_ptr<btCollisionShape> CreateBoxShape(const glm::vec3& halfExtents);
    std::shared_ptr<btCollisionShape> CreateSphereShape(float radius);
    std::shared_ptr<btCollisionShape> CreateCapsuleShape(float radius, float height);
    std::shared_ptr<btCollisionShape> CreateCylinderShape(const glm::vec3& halfExtents);
    std::shared_ptr<btCollisionShape> CreateConvexHullShape(std::shared_ptr<Rendering::Mesh> mesh);
    std::shared_ptr<btCollisionShape> CreateBvhTriangleMeshShape(std::shared_ptr<Rendering::Mesh> mesh);
    
    // Rigid body factory delegates
    btRigidBody* CreateRigidBody(
        const glm::vec3& position,
        const glm::quat& rotation,
        std::shared_ptr<btCollisionShape> shape,
        float mass = 0.0f,
        const glm::vec3& inertia = glm::vec3(0.0f)
    );
    void DestroyRigidBody(btRigidBody* body);
    bool HasRigidBody(btRigidBody* body) const;
    
    // Character controller factory delegates
    btKinematicCharacterController* CreateCharacterController(
        const glm::vec3& position,
        float radius,
        float height
    );
    void DestroyCharacterController(btKinematicCharacterController* controller);
    bool HasCharacterController(btKinematicCharacterController* controller) const;
    
    // Soft body delegates
    void AddSoftBody(btSoftBody* softBody);
    void RemoveSoftBody(btSoftBody* softBody);
    bool HasSoftBody(btSoftBody* softBody) const;
    btSoftBodyWorldInfo& GetSoftBodyWorldInfo();

    // Constraints (for rigid-cluster "jelly" approximations, etc.)
    void AddConstraint(btTypedConstraint* constraint, bool disableCollisionsBetweenLinkedBodies = true);
    
    // Query delegates
    RaycastResult Raycast(const glm::vec3& from, const glm::vec3& to);
    RaycastResult RaycastClosest(const glm::vec3& from, const glm::vec3& to);
    SweepResult SweepTest(
        std::shared_ptr<btCollisionShape> shape,
        const glm::vec3& from,
        const glm::vec3& to,
        const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
    );
    
    // Debug access (for debug rendering)
    btDynamicsWorld* GetWorld() const { return m_world.get(); }
    btSoftRigidDynamicsWorld* GetSoftWorld() const { return m_world.get(); }
    
    // Settings
    void SetGravity(const glm::vec3& gravity);
    glm::vec3 GetGravity() const;
    
    // Internal step callbacks (for sync)
    void OnPreStep();
    void OnPostStep();

private:
    // Bullet3 core objects
    std::unique_ptr<btBroadphaseInterface> m_broadphase;
    std::unique_ptr<btCollisionConfiguration> m_collisionConfig;
    std::unique_ptr<btDispatcher> m_dispatcher;
    std::unique_ptr<btConstraintSolver> m_solver;
    std::unique_ptr<btSoftRigidDynamicsWorld> m_world; // Standard soft-rigid world (no deformable pipeline)
    std::unique_ptr<btSoftBodyWorldInfo> m_softBodyWorldInfo;

    // Track constraints we create (for jelly spring clusters, etc.)
    std::vector<btTypedConstraint*> m_constraints;
};

} // namespace Physics
} // namespace OpenEye
