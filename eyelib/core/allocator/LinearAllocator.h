#pragma once

#include "core/allocator/Allocator.h"
#include "core/Memory.h"
#include <cstddef>
#include <cstdlib>

namespace eyelib {
namespace core {
namespace allocator {

/** Stack-like linear allocator. Fast alloc; only Reset() frees. */
class LinearAllocator : public IAllocator {
public:
    explicit LinearAllocator(size_t size);
    ~LinearAllocator();

    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;

    void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void Deallocate(void* ptr, size_t size) override;
    void Reset() override;
    size_t GetUsedMemory() const override { return m_used; }
    size_t GetTotalMemory() const override { return m_size; }

private:
    void* m_memory = nullptr;
    size_t m_size = 0;
    size_t m_used = 0;
};

} // namespace allocator
} // namespace core
} // namespace eyelib
