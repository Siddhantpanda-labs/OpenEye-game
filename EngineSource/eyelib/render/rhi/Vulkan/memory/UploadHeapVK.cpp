#include "eyelib/render/rhi/Vulkan/memory/UploadHeapVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include "eyelib/render/rhi/Vulkan/memory/vk_mem_alloc.h"

namespace eyelib {
namespace render {
namespace rhi {

bool UploadHeapVK::Create(UploadHeapVK* out, void* allocator, size_t bufferSizeInBytes) {
    if (!out || !allocator || bufferSizeInBytes == 0) return false;
    if (bufferSizeInBytes < kMaxFrames) return false;  // at least one byte per region
    VmaAllocator vma = static_cast<VmaAllocator>(allocator);
    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = bufferSizeInBytes;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocResult = {};
    VkResult res = vmaCreateBuffer(vma, &bufInfo, &allocInfo, &buffer, &allocation, &allocResult);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("UploadHeapVK::Create: vmaCreateBuffer failed (0x%x)", res);
        return false;
    }
    out->m_buffer = buffer;
    out->m_allocation = allocation;
    out->m_mapped = allocResult.pMappedData;
    out->m_size = bufferSizeInBytes;
    out->m_regionSize = bufferSizeInBytes / kMaxFrames;
    for (uint32_t i = 0; i < kMaxFrames; ++i)
        out->m_writeHeadPerFrame[i] = i * out->m_regionSize;
    return true;
}

void UploadHeapVK::Destroy(UploadHeapVK* heap, void* allocator) {
    if (!heap) return;
    if (allocator && heap->m_buffer && heap->m_allocation) {
        VmaAllocator vma = static_cast<VmaAllocator>(allocator);
        vmaDestroyBuffer(vma, static_cast<VkBuffer>(heap->m_buffer), static_cast<VmaAllocation>(heap->m_allocation));
    }
    heap->m_buffer = nullptr;
    heap->m_allocation = nullptr;
    heap->m_mapped = nullptr;
    heap->m_size = 0;
    heap->m_regionSize = 0;
}

void UploadHeapVK::BeginFrame(UploadHeapVK* heap, void* device, void* fence, uint32_t frameIndex) {
    if (!heap || !device || !fence || frameIndex >= kMaxFrames) return;
    VkDevice dev = static_cast<VkDevice>(device);
    VkFence f = static_cast<VkFence>(fence);
    vkWaitForFences(dev, 1, &f, VK_TRUE, UINT64_MAX);
    heap->m_writeHeadPerFrame[frameIndex] = frameIndex * heap->m_regionSize;
}

bool UploadHeapVK::Allocate(UploadHeapVK* heap, uint32_t frameIndex, size_t size, size_t alignment, size_t* outOffset, void** outMappedPtr) {
    if (!heap || !outOffset || !outMappedPtr || size == 0 || heap->m_mapped == nullptr || frameIndex >= kMaxFrames) return false;
    if (alignment == 0) alignment = 1;
    size_t base = frameIndex * heap->m_regionSize;
    size_t regionEnd = base + heap->m_regionSize;
    size_t alignMask = alignment - 1;
    size_t head = heap->m_writeHeadPerFrame[frameIndex];
    size_t alignedHead = (head + alignMask) & ~alignMask;
    if (alignedHead + size > regionEnd) return false;
    *outOffset = alignedHead;
    *outMappedPtr = static_cast<char*>(heap->m_mapped) + alignedHead;
    heap->m_writeHeadPerFrame[frameIndex] = alignedHead + size;
    return true;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
