#include "api/physics/CollisionShape.h"

namespace eyelib {
namespace api {
namespace physics {

// Stub: shape creation is via PhysicsWorld::CreateBoxShape etc. (backend-owned). No standalone shape factory.
CollisionShapeHandle CollisionShape::CreateBox(const eyelib::math::Vec3& /*halfExtents*/) { return 0; }
CollisionShapeHandle CollisionShape::CreateSphere(float /*radius*/) { return 0; }
CollisionShapeHandle CollisionShape::CreateCapsule(float /*radius*/, float /*height*/) { return 0; }
CollisionShapeHandle CollisionShape::CreateCylinder(const eyelib::math::Vec3& /*halfExtents*/) { return 0; }
CollisionShapeHandle CollisionShape::CreateCone(float /*radius*/, float /*height*/) { return 0; }
CollisionShapeHandle CollisionShape::CreateConvexHull(const std::vector<eyelib::math::Vec3>& /*vertices*/) { return 0; }
CollisionShapeHandle CollisionShape::CreateTriangleMesh(const std::vector<eyelib::math::Vec3>& /*vertices*/,
                                                        const std::vector<uint32_t>& /*indices*/) { return 0; }
CollisionShapeHandle CollisionShape::CreateHeightfield(int /*width*/, int /*length*/, const float* /*data*/,
                                                       float /*minHeight*/, float /*maxHeight*/,
                                                       int /*upAxis*/, bool /*flipQuadEdges*/) { return 0; }
CollisionShapeHandle CollisionShape::CreateCompound(const std::vector<CollisionShapeHandle>& /*shapes*/,
                                                    const std::vector<eyelib::math::Mat4>& /*transforms*/) { return 0; }
void CollisionShape::Destroy(CollisionShapeHandle /*handle*/) {}

} // namespace physics
} // namespace api
} // namespace eyelib
