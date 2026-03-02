#pragma once

#include "eyelib/world/systems/IWorldSystem.h"
#include "eyelib/world/WorldState.h"

namespace eyelib {
namespace world {
class World;
namespace systems {

class RegionSystem : public IWorldSystem {
public:
    std::string Name() const override { return "region"; }
    SchedulePhase Phase() const override { return SchedulePhase::PreSimulation; }
    bool FixedStep() const override { return false; }

    void Update(World& world, float dt) override;
    void Update(WorldState& state) const;
};

} // namespace systems
} // namespace world
} // namespace eyelib
