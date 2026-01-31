#pragma once

#include <functional>
#include <string>
#include <vector>

namespace eyelib {
namespace world {

class WorldScheduler {
public:
    using StepFn = std::function<void(float)>;
    enum class TimeMode {
        Realtime,
        Fixed,
        Replay
    };

    void AddStep(const std::string& name, StepFn fn);
    void AddStep(const std::string& name, int priority, StepFn fn);
    void AddStep(const std::string& name, const std::string& group, int priority, StepFn fn);
    void ClearSteps();

    void SetFixedTimestep(float seconds);
    float GetFixedTimestep() const;
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
    struct Step {
        std::string name;
        std::string group;
        int priority = 0;
        StepFn fn;
    };

    std::vector<Step> m_steps;
    bool m_paused = false;
    bool m_stepOnce = false;
    float m_fixedTimestep = 0.0f;
    float m_accumulator = 0.0f;
    int m_maxSubsteps = 8;
    float m_timeScale = 1.0f;
    TimeMode m_timeMode = TimeMode::Realtime;
    float m_replayDelta = 0.0f;
};

} // namespace world
} // namespace eyelib
