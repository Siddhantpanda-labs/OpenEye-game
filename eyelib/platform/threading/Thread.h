#pragma once

#include <thread>
#include <functional>
#include <memory>
#include <utility>

namespace eyelib {
namespace platform {
namespace threading {

/**
 * @brief Thread wrapper. Owns a std::thread; joinable until Joined or moved.
 */
class Thread {
public:
    Thread() = default;

    template <typename Fn, typename... Args>
    explicit Thread(Fn&& fn, Args&&... args)
        : m_impl(std::make_unique<std::thread>(std::forward<Fn>(fn), std::forward<Args>(args)...)) {}

    Thread(Thread&&) noexcept = default;
    Thread& operator=(Thread&&) noexcept = default;
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    void Join() {
        if (m_impl && m_impl->joinable())
            m_impl->join();
    }

    void Detach() {
        if (m_impl && m_impl->joinable())
            m_impl->detach();
    }

    bool Joinable() const { return m_impl && m_impl->joinable(); }

    std::thread::id GetId() const { return m_impl ? m_impl->get_id() : std::thread::id{}; }

    static unsigned int HardwareConcurrency() { return std::thread::hardware_concurrency(); }

private:
    std::unique_ptr<std::thread> m_impl;
};

} // namespace threading
} // namespace platform
} // namespace eyelib
