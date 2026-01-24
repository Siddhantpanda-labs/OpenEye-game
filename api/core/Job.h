#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <functional>

#include "eyelib/core/job/ParallelFor.h"

namespace api {

/**
 * @brief Thread pool API. Submit tasks, wait for completion.
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 0);
    ~ThreadPool();
    ThreadPool(ThreadPool&&) noexcept;
    ThreadPool& operator=(ThreadPool&&) noexcept;
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void Submit(std::function<void()> task);
    void SubmitBatch(const std::vector<std::function<void()>>& tasks);
    void Wait();
    size_t GetThreadCount() const;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

/**
 * @brief Task graph API. AddTask(name, fn, deps), Execute() runs in dependency order.
 */
class TaskGraph {
public:
    explicit TaskGraph(size_t threadCount = 0);
    ~TaskGraph();
    TaskGraph(TaskGraph&&) noexcept;
    TaskGraph& operator=(TaskGraph&&) noexcept;
    TaskGraph(const TaskGraph&) = delete;
    TaskGraph& operator=(const TaskGraph&) = delete;

    void AddTask(const std::string& name, std::function<void()> task,
                 const std::vector<std::string>& dependencies = {});
    void Execute();

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

void ParallelFor(size_t begin, size_t end, std::function<void(size_t)> fn);

template <typename Container, typename Fn>
void ParallelForEach(Container& container, Fn fn) {
    eyelib::core::job::ParallelForEach(container, std::move(fn));
}

} // namespace api
