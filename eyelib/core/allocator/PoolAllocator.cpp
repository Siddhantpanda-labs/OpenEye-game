#include "core/allocator/PoolAllocator.h"

namespace eyelib {
namespace core {
namespace allocator {

PoolAllocator::PoolAllocator(size_t blockSize, size_t blockCount)
    : m_blockSize(blockSize >= sizeof(Block*) ? blockSize : sizeof(Block*)),
      m_blockCount(blockCount) {
    m_memory = std::malloc(m_blockSize * m_blockCount);
    if (!m_memory) return;
    char* p = static_cast<char*>(m_memory);
    m_freeList = reinterpret_cast<Block*>(p);
    Block* cur = m_freeList;
    for (size_t i = 0; i < m_blockCount - 1; ++i) {
        cur->next = reinterpret_cast<Block*>(p + (i + 1) * m_blockSize);
        cur = cur->next;
    }
    cur->next = nullptr;
}

PoolAllocator::~PoolAllocator() {
    std::free(m_memory);
    m_memory = nullptr;
    m_freeList = nullptr;
}

void* PoolAllocator::Allocate(size_t size, size_t alignment) {
    (void)alignment;
    if (!m_freeList || size > m_blockSize) return nullptr;
    Block* b = m_freeList;
    m_freeList = m_freeList->next;
    ++m_allocated;
    return b;
}

void PoolAllocator::Deallocate(void* ptr, size_t size) {
    (void)size;
    if (!ptr) return;
    Block* b = static_cast<Block*>(ptr);
    b->next = m_freeList;
    m_freeList = b;
    --m_allocated;
}

void PoolAllocator::Reset() {
    if (!m_memory) return;
    char* p = static_cast<char*>(m_memory);
    m_freeList = reinterpret_cast<Block*>(p);
    Block* cur = m_freeList;
    for (size_t i = 0; i < m_blockCount - 1; ++i) {
        cur->next = reinterpret_cast<Block*>(p + (i + 1) * m_blockSize);
        cur = cur->next;
    }
    cur->next = nullptr;
    m_allocated = 0;
}

size_t PoolAllocator::GetUsedMemory() const {
    return m_allocated * m_blockSize;
}

} // namespace allocator
} // namespace core
} // namespace eyelib
