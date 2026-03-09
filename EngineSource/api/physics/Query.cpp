#include "api/physics/Query.h"
#include "api/physics/IPhysicsWorld.h"

namespace eyelib {
namespace api {
namespace physics {

RaycastResult Query::Raycast(PhysicsWorld& world, const eyelib::math::Vec3& from, const eyelib::math::Vec3& to) {
    IPhysicsWorld* backend = world.GetInternal();
    return backend ? backend->Raycast(from, to) : RaycastResult{};
}

std::vector<RaycastResult> Query::RaycastAll(PhysicsWorld& /*world*/, const eyelib::math::Vec3& /*from*/, const eyelib::math::Vec3& /*to*/) {
    return {};
}

RaycastResult Query::RaycastClosest(PhysicsWorld& world, const eyelib::math::Vec3& from, const eyelib::math::Vec3& to) {
    return Query::Raycast(world, from, to);
}

SweepResult Query::SweepTest(PhysicsWorld& /*world*/, CollisionShapeHandle /*shape*/,
                             const eyelib::math::Vec3& /*from*/, const eyelib::math::Vec3& /*to*/,
                             const eyelib::math::Quat& /*rotation*/) {
    return {};
}

std::vector<OverlapResult> Query::OverlapTest(PhysicsWorld& /*world*/, CollisionShapeHandle /*shape*/,
                                              const eyelib::math::Vec3& /*position*/, const eyelib::math::Quat& /*rotation*/) {
    return {};
}

std::vector<RigidBodyHandle> Query::QueryAABB(PhysicsWorld& world, const eyelib::math::AABB& aabb) {
    IPhysicsWorld* backend = world.GetInternal();
    return backend ? backend->QueryAABB(aabb) : std::vector<RigidBodyHandle>{};
}

std::vector<RigidBodyHandle> Query::QuerySphere(PhysicsWorld& /*world*/, const eyelib::math::Vec3& /*center*/, float /*radius*/) {
    return {};
}

} // namespace physics
} // namespace api
} // namespace eyelib
