#include "eyelib/render/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/render/rhi/Vulkan/memory/vk_mem_alloc.h"
#include "eyelib/core/Log.h"
#include <cstring>
#include <vulkan/vulkan.h>

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

IBuffer* CreateBufferVK(DeviceVK* deviceObj, const BufferDesc& desc, const void* initialData) {
    if (!deviceObj || desc.sizeInBytes == 0) return nullptr;
    VmaAllocator allocator = static_cast<VmaAllocator>(deviceObj->GetAllocator());
    if (!allocator) return nullptr;

    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = desc.sizeInBytes;
    bufInfo.usage = ToVkBufferUsage(desc.usage);

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = (desc.memoryHint == BufferMemoryHint::HostVisible || desc.memoryHint == BufferMemoryHint::HostVisiblePersistent)
        ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY;
    if (desc.memoryHint == BufferMemoryHint::HostVisiblePersistent)
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer vkBuffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkResult res = vmaCreateBuffer(allocator, &bufInfo, &allocCreateInfo, &vkBuffer, &allocation, nullptr);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("CreateBufferVK: vmaCreateBuffer failed (0x%x)", res);
        return nullptr;
    }

    auto* b = new BufferVK(deviceObj);
    b->m_size = desc.sizeInBytes;
    b->m_hint = desc.memoryHint;
    b->m_buffer = vkBuffer;
    b->m_allocation = allocation;

    if (initialData && desc.sizeInBytes > 0 &&
        (desc.memoryHint == BufferMemoryHint::HostVisible || desc.memoryHint == BufferMemoryHint::HostVisiblePersistent)) {
        void* mapped = nullptr;
        if (vmaMapMemory(allocator, allocation, &mapped) == VK_SUCCESS) {
            memcpy(mapped, initialData, desc.sizeInBytes);
            vmaUnmapMemory(allocator, allocation);
        }
    }
    return b;
}

void DestroyBufferVK(DeviceVK* deviceObj, IBuffer* buffer) {
    if (!deviceObj || !buffer) return;
    BufferVK* b = static_cast<BufferVK*>(buffer);
    VmaAllocator allocator = static_cast<VmaAllocator>(deviceObj->GetAllocator());
    if (allocator && b->m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, static_cast<VkBuffer>(b->m_buffer), static_cast<VmaAllocation>(b->m_allocation));
        b->m_buffer = nullptr;
        b->m_allocation = nullptr;
    }
    delete b;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
