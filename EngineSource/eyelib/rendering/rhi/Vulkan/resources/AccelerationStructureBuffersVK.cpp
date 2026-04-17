#include "eyelib/rendering/rhi/Vulkan/resources/AccelerationStructureBuffersVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

#if defined(VK_KHR_acceleration_structure)
static_assert(sizeof(VkAccelerationStructureInstanceKHR) == kAccelerationStructureInstanceRecordStrideBytes,
    "Update kAccelerationStructureInstanceRecordStrideBytes in RHITypes.h to match VkAccelerationStructureInstanceKHR.");
#endif

AccelerationStructureScratchBufferVK::AccelerationStructureScratchBufferVK(DeviceVK* device) : BufferVK(device) {}

AccelerationStructureInstanceBufferVK::AccelerationStructureInstanceBufferVK(DeviceVK* device, uint32_t maxInstanceCount, size_t strideBytes)
    : BufferVK(device), m_maxInstanceCount(maxInstanceCount), m_strideBytes(strideBytes) {}

namespace {

static uint64_t AlignScratchSizeBytes(DeviceVK* device, uint64_t size) {
    const uint64_t a = device->GetRayTracingScratchAlignment();
    if (a <= 1u || size == 0)
        return size;
    const uint64_t align = static_cast<uint64_t>(a);
    return (size + align - 1u) / align * align;
}

} // namespace

IAccelerationStructureScratchBuffer* CreateAccelerationStructureScratchBufferVK(DeviceVK* device, const AccelerationStructureScratchBufferDesc& desc,
                                                                                const void* initialData) {
#if !defined(VK_KHR_acceleration_structure)
    (void)device;
    (void)desc;
    (void)initialData;
    return nullptr;
#else
    if (!device || !device->SupportsRayQuery()) {
        core::Log::Error("CreateAccelerationStructureScratchBufferVK: ray query / acceleration structure slice is not enabled.");
        return nullptr;
    }
    if (desc.sizeInBytes == 0) {
        core::Log::Error("CreateAccelerationStructureScratchBufferVK: sizeInBytes must be non-zero.");
        return nullptr;
    }
    const uint64_t aligned = AlignScratchSizeBytes(device, static_cast<uint64_t>(desc.sizeInBytes));

    VkBufferUsageFlags vkUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    auto* buf = new AccelerationStructureScratchBufferVK(device);
    if (!buf->InitFromVkUsage(device, static_cast<size_t>(aligned), vkUsage, desc.memoryHint, initialData, nullptr, ResourceState::AsBuildScratch)) {
        delete buf;
        return nullptr;
    }
    if (desc.debugName && *desc.debugName)
        SetDebugNameBuffer(device, buf->m_buffer, desc.debugName);
    return buf;
#endif
}

IAccelerationStructureInstanceBuffer* CreateAccelerationStructureInstanceBufferVK(DeviceVK* device, const AccelerationStructureInstanceBufferDesc& desc,
                                                                                    const void* initialData) {
#if !defined(VK_KHR_acceleration_structure)
    (void)device;
    (void)desc;
    (void)initialData;
    return nullptr;
#else
    if (!device || !device->SupportsRayQuery()) {
        core::Log::Error("CreateAccelerationStructureInstanceBufferVK: ray query / acceleration structure slice is not enabled.");
        return nullptr;
    }
    if (desc.maxInstanceCount == 0) {
        core::Log::Error("CreateAccelerationStructureInstanceBufferVK: maxInstanceCount must be non-zero.");
        return nullptr;
    }
    const size_t stride = desc.strideInBytes != 0 ? desc.strideInBytes : kAccelerationStructureInstanceRecordStrideBytes;
    if (stride < sizeof(VkAccelerationStructureInstanceKHR)) {
        core::Log::ErrorF(
            "CreateAccelerationStructureInstanceBufferVK: strideInBytes (%zu) is smaller than VkAccelerationStructureInstanceKHR (%zu).",
            stride,
            sizeof(VkAccelerationStructureInstanceKHR));
        return nullptr;
    }
    const size_t totalBytes = static_cast<size_t>(desc.maxInstanceCount) * stride;
    if (desc.maxInstanceCount != 0 && totalBytes / desc.maxInstanceCount != stride) {
        core::Log::Error("CreateAccelerationStructureInstanceBufferVK: instance buffer byte size overflow.");
        return nullptr;
    }

    VkBufferUsageFlags vkUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
#if defined(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR)
    vkUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
#endif

    auto* buf = new AccelerationStructureInstanceBufferVK(device, desc.maxInstanceCount, stride);
    if (!buf->InitFromVkUsage(device, totalBytes, vkUsage, desc.memoryHint, initialData, nullptr, ResourceState::AsBuildGeometryInput)) {
        delete buf;
        return nullptr;
    }
    if (desc.debugName && *desc.debugName)
        SetDebugNameBuffer(device, buf->m_buffer, desc.debugName);
    return buf;
#endif
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
