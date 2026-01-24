#pragma once

#include <cstdint>
#include <vector>
#include "eyelib/math/Types.h"
#include "eyelib/physics/shapes/CollisionShapeFactory.h"

namespace eyelib {
namespace api {

using CollisionShapeHandle = eyelib::physics::CollisionShapeHandle;

/**
 * @brief Collision shape API (opaque handles).
 */
class CollisionShape {
public:
    static CollisionShapeHandle CreateBox(const eyelib::math::Vec3& halfExtents);
    static CollisionShapeHandle CreateSphere(float radius);
    static CollisionShapeHandle CreateCapsule(float radius, float height);
    static CollisionShapeHandle CreateCylinder(const eyelib::math::Vec3& halfExtents);
    static CollisionShapeHandle CreateCone(float radius, float height);
    static CollisionShapeHandle CreateConvexHull(const std::vector<eyelib::math::Vec3>& vertices);
    static CollisionShapeHandle CreateTriangleMesh(const std::vector<eyelib::math::Vec3>& vertices,
                                                   const std::vector<uint32_t>& indices);
    static CollisionShapeHandle CreateHeightfield(int width, int length, const float* data,
                                                  float minHeight, float maxHeight,
                                                  int upAxis = 1, bool flipQuadEdges = false);
    static CollisionShapeHandle CreateCompound(const std::vector<CollisionShapeHandle>& shapes,
                                               const std::vector<eyelib::math::Mat4>& transforms);
    static void Destroy(CollisionShapeHandle handle);
};

} // namespace api
} // namespace eyelib
