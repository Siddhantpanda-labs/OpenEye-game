#pragma once

#include <functional>
#include <string>
#include <vector>

namespace eyelib {
namespace world {

/** Explicit phases; order is fixed. */
enum class SchedulePhase {
    ProcessCommands,  // Scheduler-owned callback; not a registered system
    PreSimulation,
    Simulation,
    PostSimulation,
    PreRender,
    RenderSnapshot   // Scheduler-owned callback; not a registered system
};

/** Descriptor for a system that runs in a phase. Dependencies define order within and across phases. */
struct SystemDescriptor {
    std::string name;
    SchedulePhase phase = SchedulePhase::Simulation;
    std::vector<std::string> runsAfter;
    std::vector<std::string> runsBefore;
    bool fixedStep = true;  // meaningful when phase == Simulation
    std::function<void(float)> execute;
};

/**
 * Dependency-resolved execution graph with explicit phases.
 * Guarantees: deterministic order, fixed timestep for Simulation phase, snapshot after all simulation.
 */
class SystemGraphScheduler {
public:
    using StepFn = std::function<void(float)>;

    /** Register a system. Fails if name duplicate, dependency cycle, or graph locked (after first Run()). */
    bool Register(SystemDescriptor descriptor);

    /** Unregister by name. No-op if graph locked. */
    void Unregister(const std::string& name);

    /** Scheduler-owned: run once per frame before any phase. Apply pending commands only; no derived system updates. */
    void SetProcessCommandsCallback(std::function<void()> fn);
    /** Scheduler-owned: run once per frame after PreRender. Capture stable world snapshot. */
    void SetCaptureSnapshotCallback(std::function<void()> fn);
    /** Per-substep hook: run after Simulation phase (TransformSystemPostPhysics) inside the fixed-step loop. Must not mutate simulation state—only enqueue events (e.g. collision callbacks). Enforced by review. */
    void SetPostPhysicsStepCallback(std::function<void()> fn);

    void SetFixedTimestep(float seconds);
    float GetFixedTimestep() const;
    void SetMaxSubsteps(int maxSubsteps);
    int GetMaxSubsteps() const;
    void SetTimeScale(float scale);
    float GetTimeScale() const;
    void SetPaused(bool paused);
    bool GetPaused() const;
    void StepOnce();

    /** Run one frame: ProcessCommands → PreSim → (Sim × N) → PostSim → PreRender → CaptureSnapshot. */
    void Run(float frameDeltaSeconds);

    /** Accumulator (remaining time) after last Run. Use with GetFixedTimestep() for interpolation alpha: alpha = 1 - accumulator/fixedStep (0 = at current step, 1 = at next). */
    double GetAccumulator() const { return m_accumulator; }

    /** Run only the Simulation phase once with the given dt. For editor/tools that need up-to-date derived state (e.g. world matrices) after applying commands, without a full frame. Does not run other phases or touch accumulator. Explicit only—no hidden updates. */
    void RunSimulationPhaseOnce(float dt);

    /** For audit: get ordered system names in a phase. Empty if not built. */
    std::vector<std::string> GetExecutionOrder(SchedulePhase phase) const;

private:
    void BuildExecutionOrder() const;
    bool HasCycle() const;
    std::vector<size_t> TopologicalOrderForSubgraph(
        const std::vector<size_t>& indices,
        const std::vector<std::vector<size_t>>& outEdges) const;
    std::vector<std::string> TopologicalOrder(SchedulePhase phase) const;

    std::vector<SystemDescriptor> m_descriptors;
    std::function<void()> m_processCommands;
    std::function<void()> m_captureSnapshot;
    std::function<void()> m_postPhysicsStep;

    float m_fixedTimestep = 1.0f / 60.0f;
    double m_accumulator = 0.0;  // double for deterministic substep count across frame rates
    int m_maxSubsteps = 8;
    float m_timeScale = 1.0f;
    bool m_paused = false;
    bool m_stepOnce = false;

    /** Cached execution order per phase (indices into m_descriptors). Updated by BuildExecutionOrder. */
    mutable std::vector<std::vector<size_t>> m_phaseOrder;
    mutable bool m_orderDirty = true;

    /** After first Run(), graph is locked; Register/Unregister no-op. Determinism over flexibility. */
    bool m_graphLocked = false;
};

} // namespace world
} // namespace eyelib
