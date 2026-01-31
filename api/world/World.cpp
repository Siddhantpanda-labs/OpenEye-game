#include "World.h"

namespace eyelib {
namespace api {
namespace world {

class World::Impl {
public:
    ::eyelib::world::World world;
};

World::World()
    : m_impl(new Impl()) {}

World::~World() {
    delete m_impl;
    m_impl = nullptr;
}

void World::SubmitCommand(std::unique_ptr<::eyelib::world::Command> command) {
    if (!m_impl) {
        return;
    }
    m_impl->world.SubmitCommand(std::move(command));
}

void World::ApplyPendingCommands() {
    if (!m_impl) {
        return;
    }
    m_impl->world.ApplyPendingCommands();
}

void World::ApplyTransaction(::eyelib::world::Transaction&& transaction) {
    if (!m_impl) {
        return;
    }
    m_impl->world.ApplyTransaction(std::move(transaction));
}

void World::Update() {
    if (!m_impl) {
        return;
    }
    m_impl->world.Update();
}

void World::Update(float deltaSeconds) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Update(deltaSeconds);
}

void World::SetFixedTimestep(float seconds) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetFixedTimestep(seconds);
}

float World::GetFixedTimestep() const {
    if (!m_impl) {
        return 0.0f;
    }
    return m_impl->world.Scheduler().GetFixedTimestep();
}

void World::SetMaxSubsteps(int maxSubsteps) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetMaxSubsteps(maxSubsteps);
}

int World::GetMaxSubsteps() const {
    if (!m_impl) {
        return 0;
    }
    return m_impl->world.Scheduler().GetMaxSubsteps();
}

void World::SetPaused(bool paused) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetPaused(paused);
}

void World::StepOnce() {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().StepOnce();
}

void World::SetTimeScale(float scale) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetTimeScale(scale);
}

float World::GetTimeScale() const {
    if (!m_impl) {
        return 0.0f;
    }
    return m_impl->world.Scheduler().GetTimeScale();
}

void World::SetTimeMode(::eyelib::world::WorldScheduler::TimeMode mode) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetTimeMode(mode);
}

::eyelib::world::WorldScheduler::TimeMode World::GetTimeMode() const {
    if (!m_impl) {
        return ::eyelib::world::WorldScheduler::TimeMode::Realtime;
    }
    return m_impl->world.Scheduler().GetTimeMode();
}

void World::SetReplayDelta(float seconds) {
    if (!m_impl) {
        return;
    }
    m_impl->world.Scheduler().SetReplayDelta(seconds);
}

bool World::UndoLastTransaction() {
    if (!m_impl) {
        return false;
    }
    return m_impl->world.UndoLastTransaction();
}

bool World::RedoLastTransaction() {
    if (!m_impl) {
        return false;
    }
    return m_impl->world.RedoLastTransaction();
}

::eyelib::world::World* World::GetInternal() const {
    return m_impl ? &m_impl->world : nullptr;
}

} // namespace world
} // namespace api
} // namespace eyelib
