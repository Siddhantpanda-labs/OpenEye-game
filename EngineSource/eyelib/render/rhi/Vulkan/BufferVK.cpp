#include "eyelib/render/rhi/Vulkan/BufferVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/vk_mem_alloc.h"
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

BufferVK::BufferVK(DeviceVK* device) : m_device(device) {}

BufferVK::~BufferVK() = default;

size_t BufferVK::GetSize() const { return m_size; }

void* BufferVK::Map(size_t offset, size_t size) {
    if (!m_device || !m_allocation) return nullptr;
    VmaAllocator allocator = static_cast<VmaAllocator>(m_device->GetAllocator());
    if (!allocator) return nullptr;
    void* ptr = nullptr;
    size_t mapSize = (size == 0) ? m_size : size;
    if (vmaMapMemory(allocator, static_cast<VmaAllocation>(m_allocation), &ptr) != VK_SUCCESS)
        return nullptr;
    return static_cast<char*>(ptr) + offset;
}

void BufferVK::Unmap() {
    if (!m_device || !m_allocation) return;
    VmaAllocator allocator = static_cast<VmaAllocator>(m_device->GetAllocator());
    if (allocator)
        vmaUnmapMemory(allocator, static_cast<VmaAllocation>(m_allocation));
}

void BufferVK::Update(const void* data, size_t offset, size_t size) {
    if (!data) return;
    size_t copySize = (size == 0) ? (m_size - offset) : size;
    if (copySize == 0 || offset + copySize > m_size) return;
    void* mapped = Map(offset, copySize);
    if (mapped) {
        memcpy(mapped, data, copySize);
        Unmap();
    }
}

} // namespace rhi
} // namespace render
} // namespace eyelib
