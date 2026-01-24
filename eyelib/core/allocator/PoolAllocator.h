#pragma once

#include "core/allocator/Allocator.h"
#include <cstddef>
#include <cstdlib>
#include <new>

namespace eyelib {
namespace core {
namespace allocator {

/** Fixed-size object pool. All blocks same size; O(1) alloc/free. */
class PoolAllocator : public IAllocator {
public:
    PoolAllocator(size_t blockSize, size_t blockCount);
    ~PoolAllocator();

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void Deallocate(void* ptr, size_t size) override;
    void Reset() override;
    size_t GetUsedMemory() const override;
    size_t GetTotalMemory() const override { return m_blockSize * m_blockCount; }

    size_t GetBlockSize() const { return m_blockSize; }
    size_t GetBlockCount() const { return m_blockCount; }

private:
    union Block {
        Block* next;
        char storage[1];
    };

    void* m_memory = nullptr;
    size_t m_blockSize = 0;
    size_t m_blockCount = 0;
    Block* m_freeList = nullptr;
    size_t m_allocated = 0;
};

} // namespace allocator
} // namespace core
} // namespace eyelib
