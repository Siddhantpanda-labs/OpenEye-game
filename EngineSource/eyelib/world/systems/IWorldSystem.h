#pragma once

#include "eyelib/world/scheduler/SystemGraphScheduler.h"

#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace world {

class World;

/**
 * Interface for a system that runs in a scheduled phase.
 * Phase discipline and dependency ordering remain deterministic; systems are modular and injectable
 * (e.g. editor-only, game-only, AI-only). WorldScheduler runs IWorldSystem instances in phases.
 */
class IWorldSystem {
public:
    virtual ~IWorldSystem() = default;

    virtual std::string Name() const = 0;
    virtual SchedulePhase Phase() const = 0;
    /** Names of systems this one runs after (optional ordering within phase). */
    virtual std::vector<std::string> Dependencies() const { return {}; }
    /** If true (default), this system runs with fixed timestep when in Simulation phase. */
    virtual bool FixedStep() const { return true; }

    virtual void Update(World& world, float dt) = 0;
};

} // namespace world
} // namespace eyelib
