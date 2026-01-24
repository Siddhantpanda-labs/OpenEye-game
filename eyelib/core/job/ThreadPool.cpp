#include "core/job/ThreadPool.h"

namespace eyelib {
namespace core {
namespace job {

ThreadPool::ThreadPool(size_t threadCount) {
    if (threadCount == 0)
        threadCount = (std::max)(size_t(1), size_t(std::thread::hardware_concurrency()));
    m_workers.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i)
        m_workers.emplace_back(&ThreadPool::WorkerLoop, this);
}

ThreadPool::~ThreadPool() {
    m_stop = true;
    m_cv.notify_all();
    for (auto& w : m_workers)
        if (w.joinable()) w.join();
}

void ThreadPool::Submit(std::function<void()> task) {
    if (!task) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push(std::move(task));
        ++m_pending;
    }
    m_cv.notify_one();
}

void ThreadPool::SubmitBatch(const std::vector<std::function<void()>>& tasks) {
    if (tasks.empty()) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& t : tasks) {
            if (t) {
                m_tasks.push(t);
                ++m_pending;
            }
        }
    }
    m_cv.notify_all();
}

void ThreadPool::Wait() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_pending == 0; });
}

void ThreadPool::WorkerLoop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
            if (m_stop && m_tasks.empty()) return;
            if (m_tasks.empty()) continue;
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        task();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            --m_pending;
        }
        m_cv.notify_all();
    }
}

} // namespace job
} // namespace core
} // namespace eyelib
