#include "WorldAABBSystem.h"

#include "eyelib/math/AABB.h"
#include "eyelib/world/ComponentStorage/TransformStore.h"
#include "eyelib/world/components/AABB.h"
#include "eyelib/world/components/Collider.h"
#include "eyelib/world/components/WorldAABB.h"

namespace eyelib {
namespace world {
namespace systems {

static math::AABB LocalAABBFromAABB(const components::AABB& a) {
    return math::AABB(a.min, a.max);
}

static math::AABB LocalAABBFromCollider(const components::Collider& c) {
    switch (c.type) {
        case components::Collider::Type::Box: {
            math::Vec3 half = c.size * 0.5f;
            return math::AABB(c.center - half, c.center + half);
        }
        case components::Collider::Type::Sphere: {
            math::Vec3 r(c.radius, c.radius, c.radius);
            return math::AABB(c.center - r, c.center + r);
        }
        case components::Collider::Type::Capsule: {
            float r = c.radius;
            float h = c.height * 0.5f;
            math::Vec3 min(c.center.x - r, c.center.y - h - r, c.center.z - r);
            math::Vec3 max(c.center.x + r, c.center.y + h + r, c.center.z + r);
            return math::AABB(min, max);
        }
    }
    return math::AABB(c.center, c.center);
}

void WorldAABBSystem::Update(WorldState& state) const {
    auto& transformStore = state.GetTransformStore();
    auto& aabbs = state.Components<components::AABB>();
    auto& colliders = state.Components<components::Collider>();
    auto& worldAabbs = state.Components<components::WorldAABB>();

    worldAabbs.Clear();
    for (const auto& [entity, ref] : transformStore.All()) {
        math::AABB local;
        const components::AABB* aabb = aabbs.TryGet(entity);
        const components::Collider* coll = colliders.TryGet(entity);
        if (aabb)
            local = LocalAABBFromAABB(*aabb);
        else if (coll)
            local = LocalAABBFromCollider(*coll);
        else
            continue;
        math::Mat4 world = transformStore.GetWorldMatrix(entity);
        math::AABB worldBox = local.Transform(world);
        worldAabbs.Add(entity, components::WorldAABB{worldBox.min, worldBox.max});
    }
}

} // namespace systems
} // namespace world
} // namespace eyelib
