#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

struct BufferVK : IBuffer {
    explicit BufferVK(DeviceVK* device);
    ~BufferVK() override;

    size_t GetSize() const override;
    void* Map(size_t offset, size_t size) override;
    void Unmap() override;
    void Update(const void* data, size_t offset, size_t size) override;

    DeviceVK* m_device = nullptr;
    size_t m_size = 0;
    BufferMemoryHint m_hint = BufferMemoryHint::DeviceOnly;
    // VkBuffer + VmaAllocation (filled in when memory system is implemented)
    void* m_buffer = nullptr;
    void* m_allocation = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
