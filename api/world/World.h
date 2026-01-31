#pragma once

#include "eyelib/world/World.h"

#include <memory>

namespace eyelib {
namespace api {
namespace world {

class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void SubmitCommand(std::unique_ptr<::eyelib::world::Command> command);
    void ApplyPendingCommands();
    void ApplyTransaction(::eyelib::world::Transaction&& transaction);
    void Update();
    void Update(float deltaSeconds);
    bool UndoLastTransaction();
    bool RedoLastTransaction();

    void SetFixedTimestep(float seconds);
    float GetFixedTimestep() const;
    void SetMaxSubsteps(int maxSubsteps);
    int GetMaxSubsteps() const;
    void SetPaused(bool paused);
    void StepOnce();
    void SetTimeScale(float scale);
    float GetTimeScale() const;
    void SetTimeMode(::eyelib::world::WorldScheduler::TimeMode mode);
    ::eyelib::world::WorldScheduler::TimeMode GetTimeMode() const;
    void SetReplayDelta(float seconds);

    ::eyelib::world::World* GetInternal() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace world
} // namespace api
} // namespace eyelib
