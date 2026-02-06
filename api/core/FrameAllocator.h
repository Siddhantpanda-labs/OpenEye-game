#pragma once

#include <cstddef>
#include <cstdint>

namespace api {

/**
 * @brief Frame allocator (linear allocator). Fast alloc, reset once per frame.
 * Falls back to heap on overflow (implementation detail in .cpp).
 */
class FrameAllocator {
public:
    explicit FrameAllocator(size_t sizeBytes = 1024 * 1024);
    ~FrameAllocator();

    FrameAllocator(FrameAllocator&& other) noexcept;
    FrameAllocator& operator=(FrameAllocator&& other) noexcept;

    FrameAllocator(const FrameAllocator&) = delete;
    FrameAllocator& operator=(const FrameAllocator&) = delete;

    void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));
    void Reset();

    size_t GetUsedMemory() const;
    size_t GetTotalMemory() const;

    template <typename T>
    T* AllocateArray(size_t count) {
        return static_cast<T*>(Allocate(sizeof(T) * count, alignof(T)));
    }

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
