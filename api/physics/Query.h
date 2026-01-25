#pragma once

#include "api/physics/PhysicsWorld.h"
#include "api/physics/CollisionShape.h"
#include "eyelib/math/AABB.h"
#include <vector>
#include "eyelib/physics/queries/PhysicsQuery.h"

namespace eyelib {
namespace api {
namespace physics {

using RaycastResult = eyelib::physics::RaycastResult;
using SweepResult = eyelib::physics::SweepResult;
using OverlapResult = eyelib::physics::OverlapResult;

class Query {
public:
    static RaycastResult Raycast(PhysicsWorld& world, const eyelib::math::Vec3& from, const eyelib::math::Vec3& to);
    static std::vector<RaycastResult> RaycastAll(PhysicsWorld& world, const eyelib::math::Vec3& from, const eyelib::math::Vec3& to);
    static RaycastResult RaycastClosest(PhysicsWorld& world, const eyelib::math::Vec3& from, const eyelib::math::Vec3& to);

    static SweepResult SweepTest(PhysicsWorld& world, CollisionShapeHandle shape,
                                 const eyelib::math::Vec3& from, const eyelib::math::Vec3& to,
                                 const eyelib::math::Quat& rotation);

    static std::vector<OverlapResult> OverlapTest(PhysicsWorld& world, CollisionShapeHandle shape,
                                                  const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation);

    static std::vector<RigidBodyHandle> QueryAABB(PhysicsWorld& world, const eyelib::math::AABB& aabb);
    static std::vector<RigidBodyHandle> QuerySphere(PhysicsWorld& world, const eyelib::math::Vec3& center, float radius);
};

} // namespace physics
} // namespace api
} // namespace eyelib
