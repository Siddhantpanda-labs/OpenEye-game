#include "WorldScheduler.h"
#include "SystemGraphScheduler.h"

namespace eyelib {
namespace world {

void WorldScheduler::SetSystemGraph(SystemGraphScheduler* scheduler) {
    m_systemGraph = scheduler;
}

void WorldScheduler::AddStep(const std::string& /*name*/, StepFn /*fn*/) {}
void WorldScheduler::AddStep(const std::string& /*name*/, int /*priority*/, StepFn /*fn*/) {}
void WorldScheduler::AddStep(const std::string& /*name*/, const std::string& /*group*/, int /*priority*/, StepFn /*fn*/) {}
void WorldScheduler::ClearSteps() {}

void WorldScheduler::SetFixedTimestep(float seconds) {
    if (m_systemGraph) m_systemGraph->SetFixedTimestep(seconds);
}

float WorldScheduler::GetFixedTimestep() const {
    return m_systemGraph ? m_systemGraph->GetFixedTimestep() : 0.0f;
}

double WorldScheduler::GetAccumulator() const {
    return m_systemGraph ? m_systemGraph->GetAccumulator() : 0.0;
}

void WorldScheduler::SetMaxSubsteps(int maxSubsteps) {
    if (m_systemGraph) m_systemGraph->SetMaxSubsteps(maxSubsteps);
}

int WorldScheduler::GetMaxSubsteps() const {
    return m_systemGraph ? m_systemGraph->GetMaxSubsteps() : 0;
}

void WorldScheduler::SetTimeScale(float scale) {
    if (m_systemGraph) m_systemGraph->SetTimeScale(scale);
}

float WorldScheduler::GetTimeScale() const {
    return m_systemGraph ? m_systemGraph->GetTimeScale() : 0.0f;
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

void WorldScheduler::SetPaused(bool paused) {
    if (m_systemGraph) m_systemGraph->SetPaused(paused);
}

void WorldScheduler::StepOnce() {
    if (m_systemGraph) m_systemGraph->StepOnce();
}

void WorldScheduler::Run(float deltaSeconds) {
    if (!m_systemGraph) return;
    float stepDelta = (m_timeMode == TimeMode::Replay) ? m_replayDelta : deltaSeconds;
    m_systemGraph->Run(stepDelta);
}

} // namespace world
} // namespace eyelib
