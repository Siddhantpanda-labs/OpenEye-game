#include "eyelib/render/rhi/Vulkan/BufferVk.h"
#include "eyelib/render/rhi/Vulkan/DeviceVk.h"
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

size_t BufferVk::GetSize() const {
    return m_size;
}

void* BufferVk::Map(size_t offset, size_t size) {
    if (!m_device || !m_allocation) return nullptr;
    VmaAllocator allocator = m_device->GetAllocator();
    if (!allocator) return nullptr;
    void* ptr = nullptr;
    if (size == 0) size = m_size;
    VkResult r = vmaMapMemory(allocator, m_allocation, &ptr);
    if (r != VK_SUCCESS) return nullptr;
    return static_cast<char*>(ptr) + offset;
}

void BufferVk::Unmap() {
    if (!m_device || !m_allocation) return;
    VmaAllocator allocator = m_device->GetAllocator();
    if (allocator) vmaUnmapMemory(allocator, m_allocation);
}

void BufferVk::Update(const void* data, size_t offset, size_t size) {
    if (!data || !m_device || !m_allocation) return;
    if (size == 0) size = m_size - offset;
    if (offset + size > m_size) return;
    VmaAllocator allocator = m_device->GetAllocator();
    if (!allocator) return;
    void* ptr = nullptr;
    VkResult r = vmaMapMemory(allocator, m_allocation, &ptr);
    if (r != VK_SUCCESS) return;
    memcpy(static_cast<char*>(ptr) + offset, data, size);
    vmaUnmapMemory(allocator, m_allocation);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
