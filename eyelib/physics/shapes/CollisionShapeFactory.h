#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>
#include <memory>
#include <vector>

class btCollisionShape;

namespace eyelib {
namespace physics {

using CollisionShapeHandle = uint32_t;

/**
 * @brief Factory for creating collision shapes. Uses opaque handles, shape caching,
 * and never exposes Bullet types. Bullet-backed implementation.
 */
class CollisionShapeFactory {
public:
    /** Primitive shapes */
    static CollisionShapeHandle CreateBox(const math::Vec3& halfExtents);
    static CollisionShapeHandle CreateSphere(float radius);
    static CollisionShapeHandle CreateCapsule(float radius, float height);
    static CollisionShapeHandle CreateCylinder(const math::Vec3& halfExtents);
    static CollisionShapeHandle CreateCone(float radius, float height);

    /**
     * Convex hull from vertex positions. No indices required.
     */
    static CollisionShapeHandle CreateConvexHull(const std::vector<math::Vec3>& vertices);

    /**
     * Static triangle mesh. For concave, non-moving geometry.
     */
    static CollisionShapeHandle CreateTriangleMesh(
        const std::vector<math::Vec3>& vertices,
        const std::vector<uint32_t>& indices);

    /**
     * Heightfield. data layout: row-major, heightStickWidth * heightStickLength floats.
     * minHeight/maxHeight define the range; upAxis: 0=X, 1=Y, 2=Z.
     */
    static CollisionShapeHandle CreateHeightfield(
        int width, int length,
        const float* data,
        float minHeight, float maxHeight,
        int upAxis = 1,
        bool flipQuadEdges = false);

    /**
     * Compound shape from child shapes and their local transforms.
     */
    static CollisionShapeHandle CreateCompound(
        const std::vector<CollisionShapeHandle>& shapes,
        const std::vector<math::Mat4>& transforms);

    static void Destroy(CollisionShapeHandle handle);

    /** Returns true if handle is valid. */
    static bool IsValid(CollisionShapeHandle handle);

    /**
     * Internal use only: resolve handle to Bullet shape. Used by RigidBodyFactory / PhysicsWorld.
     * Returns nullptr if invalid.
     */
    static ::btCollisionShape* GetShape(CollisionShapeHandle handle);

private:
    CollisionShapeFactory() = delete;
};

} // namespace physics
} // namespace eyelib
