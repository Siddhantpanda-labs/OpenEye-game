#include "api/physics\Query.h"

namespace eyelib {
namespace api {

RaycastResult Query::Raycast(PhysicsWorld& world, const eyelib::math::Vec3& from, const eyelib::math::Vec3& to) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::PhysicsQuery::Raycast(w, from, to) : RaycastResult{};
}

std::vector<RaycastResult> Query::RaycastAll(PhysicsWorld& world, const eyelib::math::Vec3& from, const eyelib::math::Vec3& to) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::PhysicsQuery::RaycastAll(w, from, to) : std::vector<RaycastResult>{};
}

RaycastResult Query::RaycastClosest(PhysicsWorld& world, const eyelib::math::Vec3& from, const eyelib::math::Vec3& to) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::PhysicsQuery::RaycastClosest(w, from, to) : RaycastResult{};
}

SweepResult Query::SweepTest(PhysicsWorld& world, CollisionShapeHandle shape,
                             const eyelib::math::Vec3& from, const eyelib::math::Vec3& to,
                             const eyelib::math::Quat& rotation) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::PhysicsQuery::SweepTest(w, shape, from, to, rotation) : SweepResult{};
}

std::vector<OverlapResult> Query::OverlapTest(PhysicsWorld& world, CollisionShapeHandle shape,
                                              const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::PhysicsQuery::OverlapTest(w, shape, position, rotation) : std::vector<OverlapResult>{};
}

std::vector<RigidBodyHandle> Query::QueryAABB(PhysicsWorld& world, const eyelib::math::AABB& aabb) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::PhysicsQuery::QueryAABB(w, aabb) : std::vector<RigidBodyHandle>{};
}

std::vector<RigidBodyHandle> Query::QuerySphere(PhysicsWorld& world, const eyelib::math::Vec3& center, float radius) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::PhysicsQuery::QuerySphere(w, center, radius) : std::vector<RigidBodyHandle>{};
}

} // namespace api
} // namespace eyelib
