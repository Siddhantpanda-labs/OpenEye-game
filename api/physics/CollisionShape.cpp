#include "api/physics/CollisionShape.h"

namespace eyelib {
namespace api {

CollisionShapeHandle CollisionShape::CreateBox(const eyelib::math::Vec3& halfExtents) {
    return eyelib::physics::CollisionShapeFactory::CreateBox(halfExtents);
}

CollisionShapeHandle CollisionShape::CreateSphere(float radius) {
    return eyelib::physics::CollisionShapeFactory::CreateSphere(radius);
}

CollisionShapeHandle CollisionShape::CreateCapsule(float radius, float height) {
    return eyelib::physics::CollisionShapeFactory::CreateCapsule(radius, height);
}

CollisionShapeHandle CollisionShape::CreateCylinder(const eyelib::math::Vec3& halfExtents) {
    return eyelib::physics::CollisionShapeFactory::CreateCylinder(halfExtents);
}

CollisionShapeHandle CollisionShape::CreateCone(float radius, float height) {
    return eyelib::physics::CollisionShapeFactory::CreateCone(radius, height);
}

CollisionShapeHandle CollisionShape::CreateConvexHull(const std::vector<eyelib::math::Vec3>& vertices) {
    return eyelib::physics::CollisionShapeFactory::CreateConvexHull(vertices);
}

CollisionShapeHandle CollisionShape::CreateTriangleMesh(const std::vector<eyelib::math::Vec3>& vertices,
                                                        const std::vector<uint32_t>& indices) {
    return eyelib::physics::CollisionShapeFactory::CreateTriangleMesh(vertices, indices);
}

CollisionShapeHandle CollisionShape::CreateHeightfield(int width, int length, const float* data,
                                                       float minHeight, float maxHeight,
                                                       int upAxis, bool flipQuadEdges) {
    return eyelib::physics::CollisionShapeFactory::CreateHeightfield(width, length, data, minHeight, maxHeight, upAxis, flipQuadEdges);
}

CollisionShapeHandle CollisionShape::CreateCompound(const std::vector<CollisionShapeHandle>& shapes,
                                                    const std::vector<eyelib::math::Mat4>& transforms) {
    return eyelib::physics::CollisionShapeFactory::CreateCompound(shapes, transforms);
}

void CollisionShape::Destroy(CollisionShapeHandle handle) {
    eyelib::physics::CollisionShapeFactory::Destroy(handle);
}

} // namespace api
} // namespace eyelib
