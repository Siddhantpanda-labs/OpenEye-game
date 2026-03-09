#include "core/job/TaskGraph.h"

namespace eyelib {
namespace core {
namespace job {

TaskGraph::TaskGraph(size_t threadCount) : m_pool(threadCount) {}

void TaskGraph::AddTask(const std::string& name, std::function<void()> task,
                        const std::vector<std::string>& dependencies) {
    Task t;
    t.fn = std::move(task);
    for (const auto& d : dependencies)
        t.deps.insert(d);
    m_tasks[name] = std::move(t);
}

std::vector<std::vector<std::string>> TaskGraph::Levelize() const {
    std::unordered_map<std::string, size_t> inDegree;
    for (const auto& [name, task] : m_tasks) {
        if (inDegree.count(name) == 0) inDegree[name] = 0;
        for (const auto& d : task.deps) {
            if (m_tasks.count(d) == 0) continue;
            ++inDegree[name];
        }
    }
    std::unordered_map<std::string, std::unordered_set<std::string>> rev;
    for (const auto& [name, task] : m_tasks) {
        for (const auto& d : task.deps) {
            if (m_tasks.count(d)) rev[d].insert(name);
        }
    }

    std::queue<std::string> q;
    for (const auto& [name, deg] : inDegree) {
        if (deg == 0) q.push(name);
    }
    std::vector<std::vector<std::string>> levels;
    while (!q.empty()) {
        std::vector<std::string> level;
        const size_t n = q.size();
        for (size_t i = 0; i < n; ++i) {
            std::string u = std::move(q.front());
            q.pop();
            level.push_back(u);
            for (const auto& v : rev[u]) {
                auto it = inDegree.find(v);
                if (it == inDegree.end()) continue;
                if (--it->second == 0) q.push(v);
            }
        }
        levels.push_back(std::move(level));
    }
    return levels;
}

void TaskGraph::Execute() {
    auto levels = Levelize();
    for (const auto& level : levels) {
        std::vector<std::function<void()>> batch;
        batch.reserve(level.size());
        for (const auto& name : level) {
            auto it = m_tasks.find(name);
            if (it == m_tasks.end() || !it->second.fn) continue;
            batch.push_back(it->second.fn);
        }
        if (batch.empty()) continue;
        m_pool.SubmitBatch(batch);
        m_pool.Wait();
    }
}

} // namespace job
} // namespace core
} // namespace eyelib
