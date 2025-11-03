#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <btBulletDynamicsCommon.h>

namespace OpenEye {
namespace Rendering { class Mesh; }
}

namespace OpenEye {
namespace Physics {

/**
 * @brief Factory for creating collision shapes.
 */
class CollisionShapeFactory {
public:
    // Primitive shapes
    static std::shared_ptr<btCollisionShape> CreateBox(const glm::vec3& halfExtents);
    static std::shared_ptr<btCollisionShape> CreateSphere(float radius);
    static std::shared_ptr<btCollisionShape> CreateCapsule(float radius, float height);
    static std::shared_ptr<btCollisionShape> CreateCylinder(const glm::vec3& halfExtents);
    
    // Mesh-based shapes
    static std::shared_ptr<btCollisionShape> CreateConvexHull(std::shared_ptr<Rendering::Mesh> mesh);
    static std::shared_ptr<btCollisionShape> CreateBvhTriangleMesh(std::shared_ptr<Rendering::Mesh> mesh);
    
    // Compound shapes (for future use)
    // static std::shared_ptr<btCollisionShape> CreateCompound(const std::vector<std::pair<std::shared_ptr<btCollisionShape>, glm::mat4>>& shapes);
};

} // namespace Physics
} // namespace OpenEye

