#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/AABB.h"
#include "eyelib/physics/PhysicsWorld.h"
#include "eyelib/physics/queries/PhysicsQueryTypes.h"
#include "eyelib/physics/shapes/CollisionShapeFactory.h"
#include <vector>

namespace eyelib {
namespace physics {

/**
 * @brief Physics query helpers (raycast, sweep, overlap).
 */
class PhysicsQuery {
public:
    static RaycastResult Raycast(PhysicsWorld* world, const math::Vec3& from, const math::Vec3& to);
    static std::vector<RaycastResult> RaycastAll(PhysicsWorld* world, const math::Vec3& from, const math::Vec3& to);
    static RaycastResult RaycastClosest(PhysicsWorld* world, const math::Vec3& from, const math::Vec3& to);

    static SweepResult SweepTest(PhysicsWorld* world, CollisionShapeHandle shape,
                                 const math::Vec3& from, const math::Vec3& to,
                                 const math::Quat& rotation);

    static std::vector<OverlapResult> OverlapTest(PhysicsWorld* world, CollisionShapeHandle shape,
                                                  const math::Vec3& position, const math::Quat& rotation);

    static std::vector<RigidBodyHandle> QueryAABB(PhysicsWorld* world, const math::AABB& aabb);
    static std::vector<RigidBodyHandle> QuerySphere(PhysicsWorld* world, const math::Vec3& center, float radius);
};

} // namespace physics
} // namespace eyelib
