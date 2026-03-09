#pragma once

#include "eyelib/world/systems/IWorldSystem.h"
#include "eyelib/world/WorldState.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace eyelib {
namespace world {
class World;
namespace systems {

/** Updates world matrices in TransformStore from local transforms and hierarchy.
 * Uses persistent linearized order (rebuilt only when hierarchy or transform set changes).
 * Dirty propagation: only recomputes entities whose local or ancestor changed.
 * Intentionally uses TransformStore (SoA) for hot-path iteration and world matrix cache, not registry views. */
class TransformSystem : public IWorldSystem {
public:
    std::string Name() const override { return "TransformSystem"; }
    SchedulePhase Phase() const override { return SchedulePhase::Simulation; }
    bool FixedStep() const override { return true; }

    void Update(World& world, float dt) override;
    void Update(WorldState& state) const;

private:
    void RebuildLinearizedOrder(WorldState& state) const;

    mutable std::vector<EntityId> m_linearizedOrder;
    mutable uint32_t m_lastHierarchyVersion = 0;
    mutable size_t m_lastTransformCount = 0;
    mutable std::vector<uint8_t> m_updatedThisPass;
};

} // namespace systems
} // namespace world
} // namespace eyelib
