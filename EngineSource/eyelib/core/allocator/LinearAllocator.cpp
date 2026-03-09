#include "core/allocator/LinearAllocator.h"

namespace eyelib {
namespace core {
namespace allocator {

LinearAllocator::LinearAllocator(size_t size) : m_size(size) {
    m_memory = std::malloc(size);
    m_used = 0;
}

LinearAllocator::~LinearAllocator() {
    std::free(m_memory);
    m_memory = nullptr;
}

void* LinearAllocator::Allocate(size_t size, size_t alignment) {
    if (!m_memory) return nullptr;
    void* current = static_cast<char*>(m_memory) + m_used;
    void* aligned = core::AlignUp(current, alignment);
    size_t offset = static_cast<char*>(aligned) - static_cast<char*>(m_memory);
    if (offset + size > m_size) return nullptr;
    m_used = offset + size;
    return aligned;
}

void LinearAllocator::Deallocate(void* ptr, size_t size) {
    (void)ptr;
    (void)size;
    /* Linear allocator does not support individual deallocation. */
}

void LinearAllocator::Reset() {
    m_used = 0;
}

} // namespace allocator
} // namespace core
} // namespace eyelib
