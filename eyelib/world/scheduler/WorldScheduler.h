#pragma once

#include <functional>
#include <string>
#include <vector>

namespace eyelib {
namespace world {

class SystemGraphScheduler;

class WorldScheduler {
public:
    using StepFn = std::function<void(float)>;
    enum class TimeMode {
        Realtime,
        Fixed,
        Replay
    };

    /** Delegates to SystemGraphScheduler. Call once (e.g. from World ctor) before using. */
    void SetSystemGraph(SystemGraphScheduler* scheduler);

    void AddStep(const std::string& name, StepFn fn);
    void AddStep(const std::string& name, int priority, StepFn fn);
    void AddStep(const std::string& name, const std::string& group, int priority, StepFn fn);
    void ClearSteps();

    void SetFixedTimestep(float seconds);
    float GetFixedTimestep() const;
    /** Accumulator after last Run. For interpolation: alpha = 1 - GetAccumulator()/GetFixedTimestep(). */
    double GetAccumulator() const;
    void SetMaxSubsteps(int maxSubsteps);
    int GetMaxSubsteps() const;

    void SetTimeScale(float scale);
    float GetTimeScale() const;
    void SetTimeMode(TimeMode mode);
    TimeMode GetTimeMode() const;
    void SetReplayDelta(float seconds);

    void SetPaused(bool paused);
    void StepOnce();

    void Run(float deltaSeconds);

private:
    SystemGraphScheduler* m_systemGraph = nullptr;
    TimeMode m_timeMode = TimeMode::Realtime;
    float m_replayDelta = 0.0f;
};

} // namespace world
} // namespace eyelib
