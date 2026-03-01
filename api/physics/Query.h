#pragma once

#include "api/physics/PhysicsWorld.h"
#include "api/physics/CollisionShape.h"
#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"

#include <vector>

namespace eyelib {
namespace api {
namespace physics {

// RaycastResult from api/physics/IPhysicsWorld.h

struct SweepResult {
    bool hit = false;
    eyelib::math::Vec3 point{0.0f};
    eyelib::math::Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
    RigidBodyHandle body{};
    float fraction = 0.0f;
};

struct OverlapResult {
    RigidBodyHandle body{};
    eyelib::math::Vec3 contactPoint{0.0f};
    eyelib::math::Vec3 normal{0.0f, 1.0f, 0.0f};
};

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
