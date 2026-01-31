#include "WorldScheduler.h"

#include <algorithm>

namespace eyelib {
namespace world {

void WorldScheduler::AddStep(const std::string& name, StepFn fn) {
    AddStep(name, "", 0, std::move(fn));
}

void WorldScheduler::AddStep(const std::string& name, int priority, StepFn fn) {
    AddStep(name, "", priority, std::move(fn));
}

void WorldScheduler::AddStep(const std::string& name, const std::string& group, int priority, StepFn fn) {
    m_steps.push_back(Step{name, group, priority, std::move(fn)});
}

void WorldScheduler::ClearSteps() {
    m_steps.clear();
}

void WorldScheduler::SetPaused(bool paused) {
    m_paused = paused;
}

void WorldScheduler::StepOnce() {
    m_stepOnce = true;
}

void WorldScheduler::SetFixedTimestep(float seconds) {
    m_fixedTimestep = seconds;
    m_accumulator = 0.0f;
}

float WorldScheduler::GetFixedTimestep() const {
    return m_fixedTimestep;
}

void WorldScheduler::SetMaxSubsteps(int maxSubsteps) {
    m_maxSubsteps = maxSubsteps;
}

int WorldScheduler::GetMaxSubsteps() const {
    return m_maxSubsteps;
}

void WorldScheduler::SetTimeScale(float scale) {
    m_timeScale = scale;
}

float WorldScheduler::GetTimeScale() const {
    return m_timeScale;
}

void WorldScheduler::SetTimeMode(TimeMode mode) {
    m_timeMode = mode;
}

WorldScheduler::TimeMode WorldScheduler::GetTimeMode() const {
    return m_timeMode;
}

void WorldScheduler::SetReplayDelta(float seconds) {
    m_replayDelta = seconds;
}

void WorldScheduler::Run(float deltaSeconds) {
    if (m_paused && !m_stepOnce) {
        return;
    }

    float stepDelta = deltaSeconds * m_timeScale;
    if (m_timeMode == TimeMode::Replay) {
        stepDelta = m_replayDelta;
    }

    std::sort(m_steps.begin(), m_steps.end(), [](const Step& a, const Step& b) {
        if (a.group != b.group) {
            return a.group < b.group;
        }
        return a.priority < b.priority;
    });

    if (m_timeMode == TimeMode::Realtime || m_fixedTimestep <= 0.0f) {
        for (auto& step : m_steps) {
            if (step.fn) {
                step.fn(stepDelta);
            }
        }
        m_stepOnce = false;
        return;
    }

    m_accumulator += stepDelta;
    int substeps = 0;
    while (m_accumulator >= m_fixedTimestep && substeps < m_maxSubsteps) {
        for (auto& step : m_steps) {
            if (step.fn) {
                step.fn(m_fixedTimestep);
            }
        }
        m_accumulator -= m_fixedTimestep;
        ++substeps;
    }

    m_stepOnce = false;
}

} // namespace world
} // namespace eyelib
