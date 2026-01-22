#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>

// Forward declarations
namespace OpenEye {
namespace Rendering { class Mesh; }
}

// Forward declarations for Bullet
class btSoftBody;
class btRigidBody;

namespace OpenEye {
namespace Physics {

class PhysicsWorld;

/**
 * @brief Factory for creating soft bodies (cloth, water, etc.).
 */
class SoftBodyFactory {
public:
    // Create cloth from mesh
    static btSoftBody* CreateCloth(
        PhysicsWorld* world,
        const glm::vec3& position,
        int widthSegments,
        int heightSegments,
        float width,
        float height,
        float mass = 1.0f
    );
    
    // Create soft body from mesh
    static btSoftBody* CreateFromMesh(
        PhysicsWorld* world,
        std::shared_ptr<void> mesh, // Actually std::shared_ptr<Rendering::Mesh>
        const glm::vec3& position,
        float mass = 1.0f
    );
    
    // Create rope/string
    static btSoftBody* CreateRope(
        PhysicsWorld* world,
        const glm::vec3& startPos,
        const glm::vec3& endPos,
        int segments,
        float mass = 0.1f
    );
    
    // Attach cloth nodes to a rigid body (for anchoring top edge to cylinder, etc.)
    static void AttachClothNodesToRigidBody(
        btSoftBody* cloth,
        btRigidBody* rigidBody,
        const std::vector<int>& nodeIndices,
        bool disableCollision = true,
        float influence = 1.0f
    );

    // Create volumetric soft body sphere (ellipsoid) for "jelly ball" / volume deformation demos
    static btSoftBody* CreateVolumeSphere(
        PhysicsWorld* world,
        const glm::vec3& position,
        float radius,
        int resolution = 512,
        float mass = 2.0f,
        float pressure = 150.0f
    );
    
    // Destroy soft body
    static void Destroy(PhysicsWorld* world, btSoftBody* softBody);
    
    // Query if soft body exists in world
    static bool ExistsInWorld(PhysicsWorld* world, btSoftBody* softBody);
};

} // namespace Physics
} // namespace OpenEye

