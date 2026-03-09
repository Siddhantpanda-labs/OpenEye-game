#include "api/core/FrameAllocator.h"

#include "eyelib/core/Log.h"
#include "eyelib/core/allocator/LinearAllocator.h"

#include <new>
#include <vector>

namespace api {

struct FrameAllocator::Impl {
    explicit Impl(size_t sizeBytes) : linear(sizeBytes) {}
    eyelib::core::allocator::LinearAllocator linear;
    struct OverflowAlloc {
        void* ptr = nullptr;
        size_t alignment = alignof(std::max_align_t);
    };
    std::vector<OverflowAlloc> overflow;
};

FrameAllocator::FrameAllocator(size_t sizeBytes) : m_impl(new Impl(sizeBytes)) {}

FrameAllocator::~FrameAllocator() {
    delete m_impl;
    m_impl = nullptr;
}

FrameAllocator::FrameAllocator(FrameAllocator&& other) noexcept : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

FrameAllocator& FrameAllocator::operator=(FrameAllocator&& other) noexcept {
    if (this == &other) return *this;
    delete m_impl;
    m_impl = other.m_impl;
    other.m_impl = nullptr;
    return *this;
}

void* FrameAllocator::Allocate(size_t size, size_t alignment) {
    if (!m_impl) return nullptr;
    void* ptr = m_impl->linear.Allocate(size, alignment);
    if (ptr) return ptr;

    // Fallback: heap allocate if frame allocator overflows.
    void* heap = ::operator new(size, std::align_val_t(alignment));
    m_impl->overflow.push_back({heap, alignment});
    eyelib::core::Log::WarnF("FrameAllocator overflow: heap %zu bytes (used=%zu total=%zu)",
                             size, m_impl->linear.GetUsedMemory(), m_impl->linear.GetTotalMemory());
    return heap;
}

void FrameAllocator::Reset() {
    if (!m_impl) return;
    for (const auto& entry : m_impl->overflow) {
        if (entry.ptr) {
            ::operator delete(entry.ptr, std::align_val_t(entry.alignment));
        }
    }
    m_impl->overflow.clear();
    m_impl->linear.Reset();
}

size_t FrameAllocator::GetUsedMemory() const {
    return m_impl ? m_impl->linear.GetUsedMemory() : 0;
}

size_t FrameAllocator::GetTotalMemory() const {
    return m_impl ? m_impl->linear.GetTotalMemory() : 0;
}

} // namespace api
