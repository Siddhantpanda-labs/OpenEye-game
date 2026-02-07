#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/Vulkan/vma/vk_mem_alloc.h"
#include <vulkan/vulkan_core.h>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVk;

struct BufferVk : IBuffer {
    size_t GetSize() const override;
    void* Map(size_t offset, size_t size) override;
    void Unmap() override;
    void Update(const void* data, size_t offset, size_t size) override;

    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = nullptr;
    size_t m_size = 0;
    BufferMemoryHint m_hint = BufferMemoryHint::DeviceOnly;
    DeviceVk* m_device = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
