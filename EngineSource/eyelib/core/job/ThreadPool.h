#pragma once

#include <cstddef>
#include <vector>
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace eyelib {
namespace core {
namespace job {

/** Worker thread pool. Submit tasks, Wait for completion. */
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void Submit(std::function<void()> task);
    void SubmitBatch(const std::vector<std::function<void()>>& tasks);
    void Wait();
    size_t GetThreadCount() const { return m_workers.size(); }

private:
    void WorkerLoop();

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_stop{false};
    std::atomic<size_t> m_pending{0};
};

} // namespace job
} // namespace core
} // namespace eyelib
