#pragma once

#include "api/core/FrameAllocator.h"

#include <cstddef>
#include <new>
#include <type_traits>

namespace api {

/**
 * @brief STL allocator adaptor for FrameAllocator (or heap fallback if nullptr).
 */
template <typename T>
class FrameAllocatorAdaptor {
public:
    using value_type = T;

    FrameAllocatorAdaptor() = default;
    explicit FrameAllocatorAdaptor(FrameAllocator* allocator) : m_allocator(allocator) {}

    template <typename U>
    FrameAllocatorAdaptor(const FrameAllocatorAdaptor<U>& other) : m_allocator(other.GetAllocator()) {}

    T* allocate(std::size_t n) {
        const size_t bytes = n * sizeof(T);
        if (m_allocator) {
            return static_cast<T*>(m_allocator->Allocate(bytes, alignof(T)));
        }
        return static_cast<T*>(::operator new(bytes));
    }

    void deallocate(T* p, std::size_t n) noexcept {
        (void)n;
        if (!m_allocator) {
            ::operator delete(p);
        }
        // FrameAllocator memory is freed on Reset(); no-op here.
    }

    FrameAllocator* GetAllocator() const { return m_allocator; }

    template <typename U>
    bool operator==(const FrameAllocatorAdaptor<U>& other) const noexcept {
        return m_allocator == other.GetAllocator();
    }

    template <typename U>
    bool operator!=(const FrameAllocatorAdaptor<U>& other) const noexcept {
        return !(*this == other);
    }

private:
    FrameAllocator* m_allocator = nullptr;
};

} // namespace api
