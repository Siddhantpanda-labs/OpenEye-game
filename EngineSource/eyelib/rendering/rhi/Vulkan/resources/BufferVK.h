#pragma once

#include "eyelib/rendering/rhi/RHI.h"
#include <vulkan/vulkan.h>

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
    /** Flush CPU-written bytes for non-coherent host-visible allocations (descriptor-buffer host writes). */
    void FlushHostVisibleWrites(size_t offset, size_t size);

    /** Shared allocator path for `CreateBufferVK` and acceleration-structure buffer factories.
     *  @param trackerSeedDesc when non-null and memory is host-visible, seeds `BarrierTrackerVK` via `ResourceStateFromBufferDescForTracker`.
     *  @param trackerInitialState when not `Undefined`, seeds the barrier tracker (overrides usage-derived state; used for AS scratch/instance buffers). */
    bool InitFromVkUsage(DeviceVK* deviceObj, size_t sizeInBytes, VkBufferUsageFlags vkUsage, BufferMemoryHint hint, const void* initialData,
                         const BufferDesc* trackerSeedDesc = nullptr, ResourceState trackerInitialState = ResourceState::Undefined);

    DeviceVK* m_device = nullptr;
    size_t m_size = 0;
    BufferMemoryHint m_hint = BufferMemoryHint::DeviceOnly;
    VkBufferUsageFlags m_vkUsage = 0; ///< Actual `VkBufferCreateInfo::usage` used at create time (includes upload `TRANSFER_DST` when applicable).
    void* m_buffer = nullptr;       // VkBuffer
    void* m_allocation = nullptr;   // VmaAllocation
    bool m_deviceAddressCapable = false;
};

IBuffer* CreateBufferVK(DeviceVK* device, const BufferDesc& desc, const void* initialData);
void DestroyBufferVK(DeviceVK* device, IBuffer* buffer);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
