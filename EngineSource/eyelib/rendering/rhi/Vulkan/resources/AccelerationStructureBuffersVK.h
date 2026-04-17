#pragma once

#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/Vulkan/resources/BufferVK.h"

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

struct AccelerationStructureScratchBufferVK final : BufferVK, IAccelerationStructureScratchBuffer {
    explicit AccelerationStructureScratchBufferVK(DeviceVK* device);
    ~AccelerationStructureScratchBufferVK() override = default;

    IBuffer* Buffer() override { return this; }
    const IBuffer* Buffer() const override { return this; }
};

struct AccelerationStructureInstanceBufferVK final : BufferVK, IAccelerationStructureInstanceBuffer {
    explicit AccelerationStructureInstanceBufferVK(DeviceVK* device, uint32_t maxInstanceCount, size_t strideBytes);
    ~AccelerationStructureInstanceBufferVK() override = default;

    IBuffer* Buffer() override { return this; }
    const IBuffer* Buffer() const override { return this; }
    uint32_t GetMaxInstanceCount() const override { return m_maxInstanceCount; }
    size_t GetInstanceStrideBytes() const override { return m_strideBytes; }

    uint32_t m_maxInstanceCount = 0;
    size_t m_strideBytes = 0;
};

IAccelerationStructureScratchBuffer* CreateAccelerationStructureScratchBufferVK(DeviceVK* device, const AccelerationStructureScratchBufferDesc& desc,
                                                                                const void* initialData);
IAccelerationStructureInstanceBuffer* CreateAccelerationStructureInstanceBufferVK(DeviceVK* device, const AccelerationStructureInstanceBufferDesc& desc,
                                                                                    const void* initialData);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
