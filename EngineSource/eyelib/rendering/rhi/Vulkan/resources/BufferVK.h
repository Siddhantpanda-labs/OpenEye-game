#pragma once

#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

/** GPU buffer backed by VMA (`VkBuffer` + `VmaAllocation`). `BufferMemoryHint` maps to CPU vs GPU memory;
 *  device-local initial data uses a one-shot upload (`SubmitDeviceLocalBufferUpload` in `BufferVK.cpp`).
 *  **Uniform buffers:** callers must size and align suballocations per `VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment`
 *  (RHI does not pad `BufferDesc::sizeInBytes` automatically). */
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
    void* m_buffer = nullptr;       // VkBuffer
    void* m_allocation = nullptr;   // VmaAllocation
};

IBuffer* CreateBufferVK(DeviceVK* device, const BufferDesc& desc, const void* initialData);
void DestroyBufferVK(DeviceVK* device, IBuffer* buffer);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
