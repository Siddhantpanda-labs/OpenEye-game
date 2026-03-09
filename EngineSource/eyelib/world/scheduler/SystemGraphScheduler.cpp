#include "SystemGraphScheduler.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace eyelib {
namespace world {

namespace {

constexpr int PhaseOrder(SchedulePhase p) {
    switch (p) {
        case SchedulePhase::ProcessCommands: return 0;
        case SchedulePhase::PreSimulation:   return 1;
        case SchedulePhase::Simulation:     return 2;
        case SchedulePhase::PostSimulation: return 3;
        case SchedulePhase::PreRender:      return 4;
        case SchedulePhase::RenderSnapshot: return 5;
    }
    return 0;
}

} // namespace

bool SystemGraphScheduler::Register(SystemDescriptor descriptor) {
    if (m_graphLocked || descriptor.name.empty() || !descriptor.execute) return false;
    for (const auto& d : m_descriptors)
        if (d.name == descriptor.name) return false;
    m_descriptors.push_back(std::move(descriptor));
    m_orderDirty = true;
    BuildExecutionOrder();
    if (HasCycle()) {
        m_descriptors.pop_back();
        m_orderDirty = true;
        return false;
    }
    return true;
}

void SystemGraphScheduler::Unregister(const std::string& name) {
    if (m_graphLocked) return;
    m_descriptors.erase(
        std::remove_if(m_descriptors.begin(), m_descriptors.end(),
            [&name](const SystemDescriptor& d) { return d.name == name; }),
        m_descriptors.end());
    m_orderDirty = true;
}

void SystemGraphScheduler::SetProcessCommandsCallback(std::function<void()> fn) {
    m_processCommands = std::move(fn);
}

void SystemGraphScheduler::SetCaptureSnapshotCallback(std::function<void()> fn) {
    m_captureSnapshot = std::move(fn);
}

void SystemGraphScheduler::SetPostPhysicsStepCallback(std::function<void()> fn) {
    m_postPhysicsStep = std::move(fn);
}

void SystemGraphScheduler::SetFixedTimestep(float seconds) {
    m_fixedTimestep = seconds;
    m_accumulator = 0.0f;
}

float SystemGraphScheduler::GetFixedTimestep() const {
    return m_fixedTimestep;
}

void SystemGraphScheduler::SetMaxSubsteps(int maxSubsteps) {
    m_maxSubsteps = maxSubsteps;
}

int SystemGraphScheduler::GetMaxSubsteps() const {
    return m_maxSubsteps;
}

void SystemGraphScheduler::SetTimeScale(float scale) {
    m_timeScale = scale;
}

float SystemGraphScheduler::GetTimeScale() const {
    return m_timeScale;
}

void SystemGraphScheduler::SetPaused(bool paused) {
    m_paused = paused;
}

bool SystemGraphScheduler::GetPaused() const {
    return m_paused;
}

void SystemGraphScheduler::StepOnce() {
    m_stepOnce = true;
}

void SystemGraphScheduler::BuildExecutionOrder() const {
    if (!m_orderDirty) return;
    m_phaseOrder.clear();
    m_phaseOrder.resize(6);

    std::unordered_map<std::string, size_t> nameToIndex;
    for (size_t i = 0; i < m_descriptors.size(); ++i)
        nameToIndex[m_descriptors[i].name] = i;

    std::vector<std::vector<size_t>> outEdges(m_descriptors.size());
    for (size_t i = 0; i < m_descriptors.size(); ++i) {
        const auto& a = m_descriptors[i];
        int orderA = PhaseOrder(a.phase);
        for (const std::string& after : a.runsAfter) {
            auto it = nameToIndex.find(after);
            if (it != nameToIndex.end()) {
                size_t j = it->second;
                if (PhaseOrder(m_descriptors[j].phase) <= orderA)
                    outEdges[j].push_back(i);
            }
        }
        for (const std::string& before : a.runsBefore) {
            auto it = nameToIndex.find(before);
            if (it != nameToIndex.end()) {
                size_t j = it->second;
                if (PhaseOrder(m_descriptors[j].phase) >= orderA)
                    outEdges[i].push_back(j);
            }
        }
        for (size_t j = 0; j < m_descriptors.size(); ++j) {
            if (i == j) continue;
            if (PhaseOrder(m_descriptors[j].phase) < orderA)
                outEdges[j].push_back(i);
        }
    }

    if (HasCycle()) return;  // leave m_orderDirty true; Register will roll back

    for (int phase = 1; phase <= 4; ++phase) {
        std::vector<size_t> indices;
        for (size_t i = 0; i < m_descriptors.size(); ++i)
            if (PhaseOrder(m_descriptors[i].phase) == phase)
                indices.push_back(i);
        std::vector<size_t> sorted = TopologicalOrderForSubgraph(indices, outEdges);
        m_phaseOrder[phase].assign(sorted.begin(), sorted.end());
    }
    m_orderDirty = false;
}

bool SystemGraphScheduler::HasCycle() const {
    std::unordered_map<std::string, size_t> nameToIndex;
    for (size_t i = 0; i < m_descriptors.size(); ++i)
        nameToIndex[m_descriptors[i].name] = i;

    std::vector<std::vector<size_t>> outEdges(m_descriptors.size());
    for (size_t i = 0; i < m_descriptors.size(); ++i) {
        const auto& a = m_descriptors[i];
        int orderA = PhaseOrder(a.phase);
        for (const std::string& after : a.runsAfter) {
            auto it = nameToIndex.find(after);
            if (it != nameToIndex.end()) {
                size_t j = it->second;
                if (PhaseOrder(m_descriptors[j].phase) <= orderA)
                    outEdges[j].push_back(i);
            }
        }
        for (const std::string& before : a.runsBefore) {
            auto it = nameToIndex.find(before);
            if (it != nameToIndex.end()) {
                size_t j = it->second;
                if (PhaseOrder(m_descriptors[j].phase) >= orderA)
                    outEdges[i].push_back(j);
            }
        }
        for (size_t j = 0; j < m_descriptors.size(); ++j) {
            if (i == j) continue;
            if (PhaseOrder(m_descriptors[j].phase) < orderA)
                outEdges[j].push_back(i);
        }
    }

    std::vector<int> color(m_descriptors.size(), 0);
    std::function<bool(size_t)> dfs = [&](size_t u) {
        color[u] = 1;
        for (size_t v : outEdges[u]) {
            if (color[v] == 1) return true;
            if (color[v] == 0 && dfs(v)) return true;
        }
        color[u] = 2;
        return false;
    };
    for (size_t i = 0; i < m_descriptors.size(); ++i)
        if (color[i] == 0 && dfs(i)) return true;
    return false;
}

std::vector<size_t> SystemGraphScheduler::TopologicalOrderForSubgraph(
    const std::vector<size_t>& indices,
    const std::vector<std::vector<size_t>>& outEdges) const {
    std::unordered_set<size_t> sub(indices.begin(), indices.end());
    std::vector<size_t> inDegree(m_descriptors.size(), 0);
    for (size_t u : indices)
        for (size_t v : outEdges[u])
            if (sub.count(v)) inDegree[v]++;
    std::vector<size_t> queue;
    for (size_t u : indices)
        if (inDegree[u] == 0) queue.push_back(u);
    std::vector<size_t> order;
    while (!queue.empty()) {
        std::sort(queue.begin(), queue.end());
        std::vector<size_t> next;
        for (size_t u : queue) {
            order.push_back(u);
            for (size_t v : outEdges[u])
                if (sub.count(v) && --inDegree[v] == 0) next.push_back(v);
        }
        queue = std::move(next);
    }
    return order;
}

std::vector<std::string> SystemGraphScheduler::TopologicalOrder(SchedulePhase phase) const {
    int p = PhaseOrder(phase);
    if (p < 1 || p > 4) return {};
    std::vector<std::string> out;
    for (size_t i : m_phaseOrder[p])
        out.push_back(m_descriptors[i].name);
    return out;
}

std::vector<std::string> SystemGraphScheduler::GetExecutionOrder(SchedulePhase phase) const {
    const_cast<SystemGraphScheduler*>(this)->BuildExecutionOrder();
    return TopologicalOrder(phase);
}

void SystemGraphScheduler::Run(float frameDeltaSeconds) {
    if (m_paused && !m_stepOnce) return;

    float dt = frameDeltaSeconds * m_timeScale;
    if (m_orderDirty) BuildExecutionOrder();

    if (m_processCommands)
        m_processCommands();

    auto runPhase = [this](int phase, float delta) {
        for (size_t i : m_phaseOrder[phase])
            if (m_descriptors[i].execute) m_descriptors[i].execute(delta);
    };

    runPhase(1, dt);
    if (m_fixedTimestep <= 0.0f) {
        runPhase(2, dt);
        if (m_postPhysicsStep) m_postPhysicsStep();
    } else {
        m_accumulator += static_cast<double>(dt);
        const double fixedStep = static_cast<double>(m_fixedTimestep);
        int substeps = 0;
        while (m_accumulator >= fixedStep && substeps < m_maxSubsteps) {
            runPhase(2, m_fixedTimestep);
            if (m_postPhysicsStep) m_postPhysicsStep();
            m_accumulator -= fixedStep;
            ++substeps;
        }
    }
    runPhase(3, dt);
    runPhase(4, dt);

    if (m_captureSnapshot)
        m_captureSnapshot();

    m_graphLocked = true;  // freeze graph after first Run(); determinism over flexibility
    m_stepOnce = false;
}

void SystemGraphScheduler::RunSimulationPhaseOnce(float dt) {
    if (m_orderDirty) BuildExecutionOrder();
    constexpr int simulationPhase = 2;
    for (size_t i : m_phaseOrder[simulationPhase])
        if (m_descriptors[i].execute) m_descriptors[i].execute(dt);
}

} // namespace world
} // namespace eyelib
