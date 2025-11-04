#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftBody.h>

namespace OpenEye {
namespace Rendering { class Mesh; }

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
        std::shared_ptr<Rendering::Mesh> mesh,
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
    
    // Destroy soft body
    static void Destroy(PhysicsWorld* world, btSoftBody* softBody);
    
    // Query if soft body exists in world
    static bool ExistsInWorld(PhysicsWorld* world, btSoftBody* softBody);
};

} // namespace Physics
} // namespace OpenEye

