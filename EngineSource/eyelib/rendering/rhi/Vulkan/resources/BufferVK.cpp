#include "eyelib/rendering/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/rendering/rhi/Vulkan/barriers/BarrierTrackerVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/rendering/rhi/Vulkan/memory/vk_mem_alloc.h"
#include "eyelib/core/Log.h"
#include <cstring>
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

namespace {

static ResourceState ResourceStateFromBufferDescForTracker(const BufferDesc& desc) {
    if (HasUsage(desc.usage, BufferUsage::IndirectDraw)) return ResourceState::IndirectDraw;
    if (HasUsage(desc.usage, BufferUsage::DescriptorBuffer)) return ResourceState::ShaderResource;
    if (HasUsage(desc.usage, BufferUsage::Storage)) return ResourceState::ShaderResource;
    if (HasUsage(desc.usage, BufferUsage::Uniform)) return ResourceState::UniformBuffer;
    if (HasUsage(desc.usage, BufferUsage::Index)) return ResourceState::IndexBuffer;
    return ResourceState::VertexBuffer;
}

/** Matches `FinalBufferAccessFromUsage` / one-shot upload final barrier semantics. */
static ResourceState ResourceStateFromVkBufferUsageForTracker(VkBufferUsageFlags u) {
    u &= static_cast<VkBufferUsageFlags>(~VK_BUFFER_USAGE_TRANSFER_DST_BIT);
#if defined(VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT)
    if (u & VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) return ResourceState::ShaderResource;
#endif
    if (u & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) return ResourceState::IndirectDraw;
    if (u & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) return ResourceState::ShaderResource;
    if (u & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) return ResourceState::UniformBuffer;
    if (u & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) return ResourceState::IndexBuffer;
    return ResourceState::VertexBuffer;
}

static void FinalBufferAccessFromUsage(VkBufferUsageFlags u, VkAccessFlags* outAccess, VkPipelineStageFlags* outStage) {
    VkAccessFlags acc = 0;
    VkPipelineStageFlags st = 0;
    if (u & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
        acc |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        st |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }
    if (u & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
        acc |= VK_ACCESS_INDEX_READ_BIT;
        st |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }
    if (u & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
        acc |= VK_ACCESS_UNIFORM_READ_BIT;
        st |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    if (u & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
        acc |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        st |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    if (u & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) {
        acc |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        st |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    }
#if defined(VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT)
    if (u & VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) {
        acc |= VK_ACCESS_SHADER_READ_BIT;
        st |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
#endif
    if (acc == 0) {
        acc = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        st = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }
    *outAccess = acc;
    *outStage = st ? st : VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
}

static bool SubmitDeviceLocalBufferUpload(DeviceVK* deviceObj, VkBuffer dst, VkDeviceSize size, const void* data,
                                          VkBufferUsageFlags usageAfterTransfer) {
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    VkQueue queue = static_cast<VkQueue>(deviceObj->GetGraphicsQueue());
    VmaAllocator allocator = static_cast<VmaAllocator>(deviceObj->GetAllocator());
    if (!device || !queue || !allocator || !data || size == 0 || dst == VK_NULL_HANDLE)
        return false;

    VkBufferCreateInfo stInfo = {};
    stInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stInfo.size = size;
    stInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stAlloc = {};
    stAlloc.usage = VMA_MEMORY_USAGE_AUTO;
    stAlloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VkBuffer staging = VK_NULL_HANDLE;
    VmaAllocation stAllocH = VK_NULL_HANDLE;
    VmaAllocationInfo stInfoOut = {};
    if (vmaCreateBuffer(allocator, &stInfo, &stAlloc, &staging, &stAllocH, &stInfoOut) != VK_SUCCESS)
        return false;
    void* mapped = stInfoOut.pMappedData;
    bool unmapStaging = false;
    if (!mapped) {
        if (vmaMapMemory(allocator, stAllocH, &mapped) != VK_SUCCESS) {
            vmaDestroyBuffer(allocator, staging, stAllocH);
            return false;
        }
        unmapStaging = true;
    }
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vmaFlushAllocation(allocator, stAllocH, 0, size);
    if (unmapStaging)
        vmaUnmapMemory(allocator, stAllocH);

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = deviceObj->GetGraphicsQueueFamily();
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        vmaDestroyBuffer(allocator, staging, stAllocH);
        return false;
    }
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocI = {};
    allocI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocI.commandPool = pool;
    allocI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocI.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &allocI, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device, pool, nullptr);
        vmaDestroyBuffer(allocator, staging, stAllocH);
        return false;
    }
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        vmaDestroyBuffer(allocator, staging, stAllocH);
        return false;
    }

    VkBufferMemoryBarrier2 barriers[2] = {};
    barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barriers[0].srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    barriers[0].srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
    barriers[0].dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].buffer = staging;
    barriers[0].offset = 0;
    barriers[0].size = size;

    barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barriers[1].srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].buffer = dst;
    barriers[1].offset = 0;
    barriers[1].size = size;

    VkDependencyInfo depToCopy = {};
    depToCopy.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depToCopy.bufferMemoryBarrierCount = 2;
    depToCopy.pBufferMemoryBarriers = barriers;
    vkCmdPipelineBarrier2(cmd, &depToCopy);

    VkBufferCopy copy = {};
    copy.size = size;
    vkCmdCopyBuffer(cmd, staging, dst, 1, &copy);

    VkAccessFlags finalAcc;
    VkPipelineStageFlags finalSt;
    FinalBufferAccessFromUsage(usageAfterTransfer & ~VK_BUFFER_USAGE_TRANSFER_DST_BIT, &finalAcc, &finalSt);

    VkBufferMemoryBarrier2 done = {};
    done.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    done.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    done.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    done.dstStageMask = static_cast<VkPipelineStageFlags2>(finalSt);
    done.dstAccessMask = static_cast<VkAccessFlags2>(finalAcc);
    done.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    done.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    done.buffer = dst;
    done.offset = 0;
    done.size = size;
    VkDependencyInfo depDone = {};
    depDone.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depDone.bufferMemoryBarrierCount = 1;
    depDone.pBufferMemoryBarriers = &done;
    vkCmdPipelineBarrier2(cmd, &depDone);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        vmaDestroyBuffer(allocator, staging, stAllocH);
        return false;
    }

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        vmaDestroyBuffer(allocator, staging, stAllocH);
        return false;
    }
    VkSubmitInfo sub = {};
    sub.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sub.commandBufferCount = 1;
    sub.pCommandBuffers = &cmd;
    VkResult sr = vkQueueSubmit(queue, 1, &sub, fence);
    if (sr != VK_SUCCESS) {
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        vmaDestroyBuffer(allocator, staging, stAllocH);
        return false;
    }
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
    vkDestroyCommandPool(device, pool, nullptr);
    vmaDestroyBuffer(allocator, staging, stAllocH);
    return true;
}

} // namespace

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

void BufferVK::FlushHostVisibleWrites(size_t offset, size_t size) {
    if (!m_device || !m_allocation) return;
    if (offset >= m_size) return;
    VmaAllocator allocator = static_cast<VmaAllocator>(m_device->GetAllocator());
    if (!allocator) return;
    VkDeviceSize flushSize = size == 0 ? static_cast<VkDeviceSize>(m_size - offset) : static_cast<VkDeviceSize>(size);
    if (flushSize == 0) return;
    vmaFlushAllocation(allocator, static_cast<VmaAllocation>(m_allocation), static_cast<VkDeviceSize>(offset), flushSize);
}

IBuffer* CreateBufferVK(DeviceVK* deviceObj, const BufferDesc& desc, const void* initialData) {
    if (!deviceObj || desc.sizeInBytes == 0) return nullptr;
    VmaAllocator allocator = static_cast<VmaAllocator>(deviceObj->GetAllocator());
    if (!allocator) return nullptr;

    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = desc.sizeInBytes;
    VkBufferUsageFlags vkUsage = ToVkBufferUsage(desc.usage);
    const bool supportsBufferDeviceAddress = deviceObj->SupportsBufferDeviceAddress();
    const bool requestsDeviceAddress = HasUsage(desc.usage, BufferUsage::ShaderDeviceAddress);
    const bool requestsDescriptorBuffer = HasUsage(desc.usage, BufferUsage::DescriptorBuffer);
    if (requestsDescriptorBuffer && !requestsDeviceAddress) {
        core::Log::Error("CreateBufferVK: DescriptorBuffer usage requires BufferUsage::ShaderDeviceAddress for Vulkan binding.");
        return nullptr;
    }
    if (requestsDeviceAddress && !supportsBufferDeviceAddress) {
        core::Log::Error("CreateBufferVK: BufferUsage::ShaderDeviceAddress requested but device does not support buffer device address.");
        return nullptr;
    }
    const bool deviceAddressCapable = (vkUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0;
    const bool deviceLocalUpload =
        initialData && desc.sizeInBytes > 0 && desc.memoryHint == BufferMemoryHint::DeviceOnly;
    if (deviceLocalUpload)
        vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.usage = vkUsage;

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
    b->m_deviceAddressCapable = deviceAddressCapable;

    if (initialData && desc.sizeInBytes > 0) {
        if (desc.memoryHint == BufferMemoryHint::HostVisible || desc.memoryHint == BufferMemoryHint::HostVisiblePersistent) {
            void* mapped = nullptr;
            if (vmaMapMemory(allocator, allocation, &mapped) == VK_SUCCESS) {
                memcpy(mapped, initialData, desc.sizeInBytes);
                vmaUnmapMemory(allocator, allocation);
            }
        } else if (desc.memoryHint == BufferMemoryHint::DeviceOnly) {
            if (!SubmitDeviceLocalBufferUpload(deviceObj, vkBuffer, static_cast<VkDeviceSize>(desc.sizeInBytes), initialData,
                                               ToVkBufferUsage(desc.usage))) {
                vmaDestroyBuffer(allocator, vkBuffer, allocation);
                delete b;
                core::Log::Error("CreateBufferVK: device-local buffer upload failed");
                return nullptr;
            }
        }
        if (BarrierTrackerVK* tr = static_cast<BarrierTrackerVK*>(deviceObj->GetBarrierTracker())) {
            if (desc.memoryHint == BufferMemoryHint::DeviceOnly)
                BarrierTrackerVK::SetBufferState(tr, vkBuffer, ResourceStateFromVkBufferUsageForTracker(vkUsage));
            else
                BarrierTrackerVK::SetBufferState(tr, vkBuffer, ResourceStateFromBufferDescForTracker(desc));
        }
    }
    return b;
}

void DestroyBufferVK(DeviceVK* deviceObj, IBuffer* buffer) {
    if (!deviceObj || !buffer) return;
    BufferVK* b = static_cast<BufferVK*>(buffer);
    if (b->m_buffer)
        BarrierTrackerVK::RemoveBufferState(static_cast<BarrierTrackerVK*>(deviceObj->GetBarrierTracker()), b->m_buffer);
    VmaAllocator allocator = static_cast<VmaAllocator>(deviceObj->GetAllocator());
    if (allocator && b->m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, static_cast<VkBuffer>(b->m_buffer), static_cast<VmaAllocation>(b->m_allocation));
        b->m_buffer = nullptr;
        b->m_allocation = nullptr;
    }
    delete b;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
