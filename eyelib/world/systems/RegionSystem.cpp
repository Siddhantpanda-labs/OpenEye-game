#include "RegionSystem.h"

#include "eyelib/world/RegionQueries.h"
#include "eyelib/world/components/RegionMembership.h"
#include "eyelib/world/components/SystemBindings.h"
#include "eyelib/world/components/ActiveSystems.h"

namespace eyelib {
namespace world {
namespace systems {

void RegionSystem::Update(WorldState& state) const {
    auto& memberships = state.Components<components::RegionMembership>();
    auto& transforms = state.GetTransformStore();
    auto& regionSystems = state.Components<components::SystemBindings>();
    auto& activeSystems = state.Components<components::ActiveSystems>();

    for (auto& [entity, membership] : memberships.AllMutable()) {
        auto* transform = transforms.TryGet(entity);
        if (!transform) {
            membership.region = InvalidEntity;
            if (auto* active = activeSystems.TryGet(entity)) {
                active->enabledSystems.clear();
            }
            continue;
        }
        auto best = GetHighestPriorityRegion(state, transform->position());  // TransformRef::position()
        membership.region = best.value_or(InvalidEntity);

        if (membership.region == InvalidEntity) {
            if (auto* active = activeSystems.TryGet(entity)) {
                active->enabledSystems.clear();
            }
            continue;
        }

        auto* bindings = regionSystems.TryGet(membership.region);
        if (!bindings) {
            if (auto* active = activeSystems.TryGet(entity)) {
                active->enabledSystems.clear();
            }
            continue;
        }

        auto* active = activeSystems.TryGet(entity);
        if (!active) {
            active = &activeSystems.Add(entity, components::ActiveSystems{});
        }
        active->enabledSystems = bindings->enabledSystems;
    }
}

} // namespace systems
} // namespace world
} // namespace eyelib
