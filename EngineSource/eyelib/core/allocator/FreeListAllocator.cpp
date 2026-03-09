#include "core/allocator/FreeListAllocator.h"

namespace eyelib {
namespace core {
namespace allocator {

FreeListAllocator::FreeListAllocator(size_t size) : m_size(size) {
    m_memory = std::malloc(size);
    if (!m_memory) return;
    m_used = 0;
    m_freeList = static_cast<BlockHeader*>(m_memory);
    m_freeList->size = size;
    m_freeList->next = nullptr;
}

FreeListAllocator::~FreeListAllocator() {
    std::free(m_memory);
    m_memory = nullptr;
    m_freeList = nullptr;
}

void* FreeListAllocator::Allocate(size_t size, size_t alignment) {
    if (!m_memory || size == 0) return nullptr;
    const size_t total = size + sizeof(BlockHeader);
    const size_t alignedTotal = (total + alignment - 1) & ~(alignment - 1);

    BlockHeader** prev = &m_freeList;
    BlockHeader* cur = m_freeList;

    while (cur) {
        char* blockStart = reinterpret_cast<char*>(cur);
        char* payloadStart = blockStart + sizeof(BlockHeader);
        void* alignedPayload = core::AlignUp(payloadStart, alignment);
        size_t prefix = static_cast<char*>(alignedPayload) - blockStart;
        size_t needed = prefix + size;
        if (needed <= cur->size) {
            const size_t remainder = cur->size - needed;
            if (remainder >= kMinBlockSize) {
                BlockHeader* rest = reinterpret_cast<BlockHeader*>(blockStart + needed);
                rest->size = remainder;
                rest->next = cur->next;
                *prev = rest;
            } else {
                *prev = cur->next;
            }
            BlockHeader* h = reinterpret_cast<BlockHeader*>(blockStart);
            h->size = needed;
            m_used += needed;
            return alignedPayload;
        }
        prev = &cur->next;
        cur = cur->next;
    }
    return nullptr;
}

void FreeListAllocator::Deallocate(void* ptr, size_t size) {
    (void)size;
    if (!ptr || !m_memory) return;
    char* p = static_cast<char*>(ptr);
    BlockHeader* h = reinterpret_cast<BlockHeader*>(p - sizeof(BlockHeader));
    m_used -= h->size;
    h->next = m_freeList;
    m_freeList = h;
}

void FreeListAllocator::Reset() {
    if (!m_memory) return;
    m_used = 0;
    m_freeList = static_cast<BlockHeader*>(m_memory);
    m_freeList->size = m_size;
    m_freeList->next = nullptr;
}

} // namespace allocator
} // namespace core
} // namespace eyelib
