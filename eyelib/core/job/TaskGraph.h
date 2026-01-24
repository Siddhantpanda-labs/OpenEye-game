#pragma once

#include "core/job/ThreadPool.h"
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <queue>

namespace eyelib {
namespace core {
namespace job {

/** Dependency-based task graph. Execute() runs levels in parallel. */
class TaskGraph {
public:
    explicit TaskGraph(size_t threadCount = 0);

    void AddTask(const std::string& name, std::function<void()> task,
                 const std::vector<std::string>& dependencies = {});
    void Execute();

private:
    std::vector<std::vector<std::string>> Levelize() const;

    struct Task {
        std::function<void()> fn;
        std::unordered_set<std::string> deps;
    };
    std::unordered_map<std::string, Task> m_tasks;
    ThreadPool m_pool;
};

} // namespace job
} // namespace core
} // namespace eyelib
