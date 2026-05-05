#include "api/physics/CollisionShape.h"

namespace eyelib {
namespace api {
namespace physics {

// Stub: shape creation is via PhysicsWorld::CreateBoxShape etc. (backend-owned). No standalone shape factory.
CollisionShapeHandle CollisionShape::CreateBox(const eyelib::math::Vec3& /*halfExtents*/) { return {}; }
CollisionShapeHandle CollisionShape::CreateSphere(float /*radius*/) { return {}; }
CollisionShapeHandle CollisionShape::CreateCapsule(float /*radius*/, float /*height*/) { return {}; }
CollisionShapeHandle CollisionShape::CreateCylinder(const eyelib::math::Vec3& /*halfExtents*/) { return {}; }
CollisionShapeHandle CollisionShape::CreateCone(float /*radius*/, float /*height*/) { return {}; }
CollisionShapeHandle CollisionShape::CreateConvexHull(const std::vector<eyelib::math::Vec3>& /*vertices*/) { return {}; }
CollisionShapeHandle CollisionShape::CreateTriangleMesh(const std::vector<eyelib::math::Vec3>& /*vertices*/,
                                                        const std::vector<uint32_t>& /*indices*/) { return {}; }
CollisionShapeHandle CollisionShape::CreateHeightfield(int /*width*/, int /*length*/, const float* /*data*/,
                                                       float /*minHeight*/, float /*maxHeight*/,
                                                       int /*upAxis*/, bool /*flipQuadEdges*/) { return {}; }
CollisionShapeHandle CollisionShape::CreateCompound(const std::vector<CollisionShapeHandle>& /*shapes*/,
                                                    const std::vector<eyelib::math::Mat4>& /*transforms*/) { return {}; }
void CollisionShape::Destroy(CollisionShapeHandle /*handle*/) {}

} // namespace physics
} // namespace api
} // namespace eyelib
