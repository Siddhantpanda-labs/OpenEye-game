#pragma once

#include "core/allocator/Allocator.h"
#include "core/Memory.h"
#include <cstddef>
#include <cstdlib>

namespace eyelib {
namespace core {
namespace allocator {

/** General-purpose allocator with variable-sized blocks and first-fit free list. */
class FreeListAllocator : public IAllocator {
public:
    explicit FreeListAllocator(size_t size);
    ~FreeListAllocator();

    FreeListAllocator(const FreeListAllocator&) = delete;
    FreeListAllocator& operator=(const FreeListAllocator&) = delete;

    void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void Deallocate(void* ptr, size_t size) override;
    void Reset() override;
    size_t GetUsedMemory() const override { return m_used; }
    size_t GetTotalMemory() const override { return m_size; }

private:
    struct BlockHeader {
        size_t size;
        BlockHeader* next;
    };

    void* m_memory = nullptr;
    size_t m_size = 0;
    size_t m_used = 0;
    BlockHeader* m_freeList = nullptr;

    static constexpr size_t kMinBlockSize = sizeof(BlockHeader) + 1;
};

} // namespace allocator
} // namespace core
} // namespace eyelib
