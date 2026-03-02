#include "RegionSystem.h"

#include "eyelib/world/World.h"
#include "eyelib/world/RegionQueries.h"
#include "eyelib/world/ComponentStorage/TransformStore.h"
#include "eyelib/world/components/Transform.h"
#include "eyelib/world/components/RegionMembership.h"
#include "eyelib/world/components/SystemBindings.h"
#include "eyelib/world/components/ActiveSystems.h"

#include <entt/entt.hpp>

namespace eyelib {
namespace world {
namespace systems {

void RegionSystem::Update(World& world, float /*dt*/) {
    Update(world.GetState());
}

void RegionSystem::Update(WorldState& state) const {
    entt::registry& reg = state.GetRegistry();
    const auto& transformStore = state.GetTransformStore();

    auto view = reg.view<components::RegionMembership>();

    for (entt::entity e : view) {
        EntityId id = state.FromEnTT(e);
        if (id == InvalidEntity) {
            continue;
        }

        math::Vec3 position;
        if (const auto* t = reg.try_get<components::Transform>(e)) {
            position = t->position;
        } else {
            const auto* ref = transformStore.TryGet(id);
            if (!ref) {
                reg.get<components::RegionMembership>(e).region = InvalidEntity;
                if (auto* active = reg.try_get<components::ActiveSystems>(e)) {
                    active->enabledSystems.clear();
                }
                continue;
            }
            position = ref->position();
        }

        auto best = GetHighestPriorityRegion(state, position);
        auto& membership = reg.get<components::RegionMembership>(e);
        membership.region = best.value_or(InvalidEntity);

        if (membership.region == InvalidEntity) {
            if (auto* active = reg.try_get<components::ActiveSystems>(e)) {
                active->enabledSystems.clear();
            }
            continue;
        }

        entt::entity regionEnTT = state.ToEnTT(membership.region);
        const auto* bindings = regionEnTT != entt::null ? reg.try_get<components::SystemBindings>(regionEnTT) : nullptr;

        if (!bindings) {
            if (auto* active = reg.try_get<components::ActiveSystems>(e)) {
                active->enabledSystems.clear();
            }
            continue;
        }

        auto& active = reg.get_or_emplace<components::ActiveSystems>(e);
        active.enabledSystems = bindings->enabledSystems;
    }
}

} // namespace systems
} // namespace world
} // namespace eyelib
