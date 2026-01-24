#include "Job.h"
#include "eyelib/core/job/ThreadPool.h"
#include "eyelib/core/job/TaskGraph.h"

namespace api {

struct ThreadPool::Impl {
    eyelib::core::job::ThreadPool pool;
    explicit Impl(size_t n) : pool(n) {}
};

ThreadPool::ThreadPool(size_t threadCount) : m_impl(new Impl(threadCount)) {}

ThreadPool::~ThreadPool() { delete m_impl; m_impl = nullptr; }

ThreadPool::ThreadPool(ThreadPool&& other) noexcept : m_impl(other.m_impl) { other.m_impl = nullptr; }

ThreadPool& ThreadPool::operator=(ThreadPool&& other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

void ThreadPool::Submit(std::function<void()> task) { m_impl->pool.Submit(std::move(task)); }

void ThreadPool::SubmitBatch(const std::vector<std::function<void()>>& tasks) {
    m_impl->pool.SubmitBatch(tasks);
}

void ThreadPool::Wait() { m_impl->pool.Wait(); }

size_t ThreadPool::GetThreadCount() const { return m_impl->pool.GetThreadCount(); }

struct TaskGraph::Impl {
    eyelib::core::job::TaskGraph graph;
    explicit Impl(size_t n) : graph(n) {}
};

TaskGraph::TaskGraph(size_t threadCount) : m_impl(new Impl(threadCount)) {}

TaskGraph::~TaskGraph() { delete m_impl; m_impl = nullptr; }

TaskGraph::TaskGraph(TaskGraph&& other) noexcept : m_impl(other.m_impl) { other.m_impl = nullptr; }

TaskGraph& TaskGraph::operator=(TaskGraph&& other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

void TaskGraph::AddTask(const std::string& name, std::function<void()> task,
                        const std::vector<std::string>& dependencies) {
    m_impl->graph.AddTask(name, std::move(task), dependencies);
}

void TaskGraph::Execute() { m_impl->graph.Execute(); }

void ParallelFor(size_t begin, size_t end, std::function<void(size_t)> fn) {
    eyelib::core::job::ParallelFor(begin, end, std::move(fn));
}

} // namespace api
