#pragma once

#include "platform/threading/Mutex.h"
#include <thread>
#include <unordered_map>

namespace eyelib {
namespace platform {
namespace threading {

/** Per-instance, per-thread storage. Each thread has its own T for this instance. */
template <typename T>
class ThreadLocal {
public:
    T& Get() {
        std::lock_guard<Mutex> lock(m_mutex);
        return m_storage[std::this_thread::get_id()];
    }
    const T& Get() const {
        std::lock_guard<Mutex> lock(m_mutex);
        return m_storage.at(std::this_thread::get_id());
    }

private:
    mutable Mutex m_mutex;
    mutable std::unordered_map<std::thread::id, T> m_storage;
};

} // namespace threading
} // namespace platform
} // namespace eyelib
