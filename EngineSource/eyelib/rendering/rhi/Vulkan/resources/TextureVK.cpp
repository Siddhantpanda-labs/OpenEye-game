#include "eyelib/rendering/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/rendering/rhi/Vulkan/barriers/BarrierTrackerVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/rendering/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/rendering/rhi/Vulkan/memory/vk_mem_alloc.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstring>

namespace eyelib {
namespace rendering {
namespace rhi {

TextureVK::TextureVK(DeviceVK* device) : m_device(device) {}

TextureVK::~TextureVK() = default;

uint32_t TextureVK::GetWidth() const { return m_width; }

uint32_t TextureVK::GetHeight() const { return m_height; }

TextureType TextureVK::GetType() const { return m_type; }

RHITextureFormat TextureVK::GetFormat() const { return m_format; }

void* TextureVK::GetNativeTexture() { return m_imageView; }

void* TextureVK::GetNativeTexture2DViewForArrayLayer(uint32_t layer) {
    if (m_retired || !m_image || m_slice2DViewsByLayer.empty())
        return nullptr;
    if (layer >= m_slice2DViewsByLayer.size())
        return nullptr;
    if (m_slice2DViewsByLayer[layer])
        return m_slice2DViewsByLayer[layer];
    VkDevice device = static_cast<VkDevice>(m_device->GetDevice());
    if (device == VK_NULL_HANDLE)
        return nullptr;
    VkImage image = static_cast<VkImage>(m_image);
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = ToVkFormat(m_format);
    viewInfo.subresourceRange.aspectMask = ToVkImageAspect(m_format);
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = m_mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = layer;
    viewInfo.subresourceRange.layerCount = 1;
    VkImageView sliceView = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &viewInfo, nullptr, &sliceView) != VK_SUCCESS)
        return nullptr;
    m_slice2DViewsByLayer[layer] = sliceView;
    return sliceView;
}

bool TextureVK::IsGpuBindable() const { return !m_retired && m_imageView != nullptr; }

namespace {

bool GetUploadLayout(RHITextureFormat fmt, uint32_t* srcBpp, uint32_t* dstBpp) {
    switch (fmt) {
    case RHITextureFormat::R8:
        *srcBpp = 1;
        *dstBpp = 1;
        return true;
    case RHITextureFormat::RG8:
        *srcBpp = 2;
        *dstBpp = 2;
        return true;
    case RHITextureFormat::RGB8:
    case RHITextureFormat::RGB8_SRGB:
        *srcBpp = 3;
        *dstBpp = 4;
        return true;
    case RHITextureFormat::RGBA8:
    case RHITextureFormat::RGBA8_SRGB:
    case RHITextureFormat::BGRA8:
        *srcBpp = 4;
        *dstBpp = 4;
        return true;
    case RHITextureFormat::RGB16F:
        *srcBpp = 6;
        *dstBpp = 8;
        return true;
    case RHITextureFormat::RGBA16F:
        *srcBpp = 8;
        *dstBpp = 8;
        return true;
    case RHITextureFormat::R32UI:
        *srcBpp = 4;
        *dstBpp = 4;
        return true;
    default:
        return false;
    }
}

void CopyToStaging(RHITextureFormat fmt, const void* src, uint32_t w, uint32_t h, uint8_t* dst, size_t dstRowPitch) {
    const auto* s = static_cast<const uint8_t*>(src);
    const uint32_t srcBpp = (fmt == RHITextureFormat::RGB8 || fmt == RHITextureFormat::RGB8_SRGB) ? 3u
        : (fmt == RHITextureFormat::R8) ? 1u
        : (fmt == RHITextureFormat::RG8) ? 2u
        : (fmt == RHITextureFormat::RGB16F) ? 6u
        : (fmt == RHITextureFormat::RGBA16F) ? 8u
        : 4u;

    if ((fmt == RHITextureFormat::RGB8 || fmt == RHITextureFormat::RGB8_SRGB) && dstRowPitch >= w * 4u) {
        for (uint32_t y = 0; y < h; ++y) {
            const uint8_t* row = s + static_cast<size_t>(y) * w * 3u;
            uint8_t* d = dst + static_cast<size_t>(y) * dstRowPitch;
            for (uint32_t x = 0; x < w; ++x) {
                d[x * 4 + 0] = row[x * 3 + 0];
                d[x * 4 + 1] = row[x * 3 + 1];
                d[x * 4 + 2] = row[x * 3 + 2];
                d[x * 4 + 3] = 255;
            }
        }
        return;
    }
    if (fmt == RHITextureFormat::RGB16F && dstRowPitch >= w * 8u) {
        static const uint8_t kOneHalf[2] = { 0x00, 0x3c }; // 1.0f fp16 LE
        for (uint32_t y = 0; y < h; ++y) {
            const uint8_t* row = s + static_cast<size_t>(y) * w * 6u;
            uint8_t* d = dst + static_cast<size_t>(y) * dstRowPitch;
            for (uint32_t x = 0; x < w; ++x) {
                std::memcpy(d + x * 8, row + x * 6, 6);
                d[x * 8 + 6] = kOneHalf[0];
                d[x * 8 + 7] = kOneHalf[1];
            }
        }
        return;
    }

    const size_t rowCopy = static_cast<size_t>(w) * srcBpp;
    for (uint32_t y = 0; y < h; ++y) {
        std::memcpy(dst + static_cast<size_t>(y) * dstRowPitch, s + static_cast<size_t>(y) * rowCopy, rowCopy);
    }
}

static uint32_t VkFormatColorTexelBytes(VkFormat f) {
    switch (f) {
    case VK_FORMAT_R8_UNORM:
        return 1u;
    case VK_FORMAT_R8G8_UNORM:
        return 2u;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
        return 4u;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return 8u;
    case VK_FORMAT_R32_UINT:
        return 4u;
    default:
        return 4u;
    }
}

static VkQueue SelectUploadQueue(DeviceVK* deviceObj, uint32_t* outQueueFamily) {
    if (!deviceObj) return VK_NULL_HANDLE;
    VkQueue queue = static_cast<VkQueue>(deviceObj->GetGraphicsQueue());
    uint32_t family = deviceObj->GetGraphicsQueueFamily();

    VkQueue transferQueue = static_cast<VkQueue>(deviceObj->GetTransferQueue());
    const uint32_t transferFamily = deviceObj->GetTransferQueueFamily();
    if (transferQueue != VK_NULL_HANDLE) {
        if (transferFamily == family) {
            queue = transferQueue;
        } else {
            static bool warnedDedicatedTransfer = false;
            if (!warnedDedicatedTransfer) {
                core::Log::Info(
                    "TextureVK upload policy: dedicated transfer queue detected but upload path remains on graphics queue "
                    "until explicit queue-family ownership transfer for images is implemented.");
                warnedDedicatedTransfer = true;
            }
        }
    }

    if (outQueueFamily) *outQueueFamily = family;
    return queue;
}

bool SubmitTextureUpload(DeviceVK* deviceObj, VkImage image, VkFormat vkFormat, uint32_t width, uint32_t height, VkBuffer staging) {
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    uint32_t queueFamily = 0;
    VkQueue queue = SelectUploadQueue(deviceObj, &queueFamily);
    if (!queue) return false;

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = queueFamily;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
        return false;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &allocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkImageMemoryBarrier2 toTransfer = {};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toTransfer.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    toTransfer.srcAccessMask = 0;
    toTransfer.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel = 0;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount = 1;
    VkDependencyInfo depToTransfer = {};
    depToTransfer.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depToTransfer.imageMemoryBarrierCount = 1;
    depToTransfer.pImageMemoryBarriers = &toTransfer;
    vkCmdPipelineBarrier2(cmd, &depToTransfer);

    const uint32_t dstBpp = VkFormatColorTexelBytes(vkFormat);
    const size_t rowPitch = (static_cast<size_t>(width) * dstBpp + 3u) & ~size_t{3};
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = static_cast<uint32_t>(rowPitch / dstBpp);
    region.bufferImageHeight = height;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };
    vkCmdCopyBufferToImage(cmd, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier2 toSample = {};
    toSample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toSample.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toSample.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toSample.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    toSample.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    toSample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.image = image;
    toSample.subresourceRange = toTransfer.subresourceRange;
    VkDependencyInfo depToSample = {};
    depToSample.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depToSample.imageMemoryBarrierCount = 1;
    depToSample.pImageMemoryBarriers = &toSample;
    vkCmdPipelineBarrier2(cmd, &depToSample);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    VkResult sr = vkQueueSubmit(queue, 1, &submit, fence);
    if (sr != VK_SUCCESS) {
        deviceObj->NoteVulkanDeviceLost("SubmitTextureUpload vkQueueSubmit", static_cast<int>(sr));
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
    vkDestroyCommandPool(device, pool, nullptr);
    return true;
}

/** Upload 6 faces (tightly packed in buffer: face * w*h*srcBpp expanded per face via rowPitch) to cube array layers 0..5. */
bool SubmitCubeTextureUpload(DeviceVK* deviceObj, VkImage image, VkFormat vkFormat, uint32_t width, uint32_t height,
                             VkBuffer staging) {
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    uint32_t queueFamily = 0;
    VkQueue queue = SelectUploadQueue(deviceObj, &queueFamily);
    if (!queue) return false;

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = queueFamily;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
        return false;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &allocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkImageMemoryBarrier2 toTransfer = {};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toTransfer.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    toTransfer.srcAccessMask = 0;
    toTransfer.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel = 0;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount = 6;
    VkDependencyInfo depToTransfer = {};
    depToTransfer.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depToTransfer.imageMemoryBarrierCount = 1;
    depToTransfer.pImageMemoryBarriers = &toTransfer;
    vkCmdPipelineBarrier2(cmd, &depToTransfer);

    const uint32_t dstBpp = VkFormatColorTexelBytes(vkFormat);
    const size_t rowPitch = (static_cast<size_t>(width) * dstBpp + 3u) & ~size_t{3};
    const VkDeviceSize faceBytes = static_cast<VkDeviceSize>(rowPitch * height);

    VkBufferImageCopy regions[6];
    for (uint32_t i = 0; i < 6; ++i) {
        regions[i] = {};
        regions[i].bufferOffset = faceBytes * static_cast<VkDeviceSize>(i);
        regions[i].bufferRowLength = static_cast<uint32_t>(rowPitch / dstBpp);
        regions[i].bufferImageHeight = height;
        regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[i].imageSubresource.mipLevel = 0;
        regions[i].imageSubresource.baseArrayLayer = i;
        regions[i].imageSubresource.layerCount = 1;
        regions[i].imageOffset = { 0, 0, 0 };
        regions[i].imageExtent = { width, height, 1 };
    }
    vkCmdCopyBufferToImage(cmd, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions);

    VkImageMemoryBarrier2 toSample = {};
    toSample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toSample.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toSample.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toSample.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    toSample.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    toSample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.image = image;
    toSample.subresourceRange = toTransfer.subresourceRange;
    VkDependencyInfo depToSample = {};
    depToSample.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depToSample.imageMemoryBarrierCount = 1;
    depToSample.pImageMemoryBarriers = &toSample;
    vkCmdPipelineBarrier2(cmd, &depToSample);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    VkResult cubeSr = vkQueueSubmit(queue, 1, &submit, fence);
    if (cubeSr != VK_SUCCESS) {
        deviceObj->NoteVulkanDeviceLost("SubmitCubeTextureUpload vkQueueSubmit", static_cast<int>(cubeSr));
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
    vkDestroyCommandPool(device, pool, nullptr);
    return true;
}

/**
 * Vulkan: images start in UNDEFINED. Descriptors for sampled images are written with
 * VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; sampling without a prior transition hits VUID-vkCmdDraw-None-09600.
 * For SRV-only textures created with no CPU upload and no render-target first use, transition the whole mip chain
 * and all array layers once. Skips RT|TransferDst textures (IBL/probes render or copy into them first).
 */
bool SubmitImageUndefinedToShaderReadOnly(DeviceVK* deviceObj, VkImage image, uint32_t mipLevels, uint32_t arrayLayers,
                                          VkImageAspectFlags aspectMask) {
    if (!image || mipLevels == 0 || arrayLayers == 0) return false;
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    uint32_t queueFamily = 0;
    VkQueue queue = SelectUploadQueue(deviceObj, &queueFamily);
    if (!queue) return false;

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = queueFamily;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
        return false;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &allocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkImageMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barrier.srcAccessMask = 0;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                           VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                           VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = arrayLayers;
    VkDependencyInfo depToSrv = {};
    depToSrv.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depToSrv.imageMemoryBarrierCount = 1;
    depToSrv.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &depToSrv);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    VkResult initSrvSr = vkQueueSubmit(queue, 1, &submit, fence);
    if (initSrvSr != VK_SUCCESS) {
        deviceObj->NoteVulkanDeviceLost("SubmitImageUndefinedToShaderReadOnly vkQueueSubmit", static_cast<int>(initSrvSr));
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, pool, 1, &cmd);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
    vkDestroyCommandPool(device, pool, nullptr);
    return true;
}

bool ValidateImageFormatUsage(VkPhysicalDevice physicalDevice, VkFormat format, VkImageUsageFlags usage) {
    if (physicalDevice == VK_NULL_HANDLE || format == VK_FORMAT_UNDEFINED) return false;
    VkFormatProperties props = {};
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
    const VkFormatFeatureFlags features = props.optimalTilingFeatures;
    auto has = [features](VkFormatFeatureFlags f) { return (features & f) == f; };
    if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) && !has(VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) return false;
    if ((usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) && !has(VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) return false;
    if ((usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) && !has(VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) return false;
    if ((usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) && !has(VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)) return false;
    if ((usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) && !has(VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) return false;
    return true;
}

} // namespace

ITexture* CreateTextureVK(DeviceVK* deviceObj, const TextureDesc& desc, const void* initialData) {
    if (!deviceObj) return nullptr;
    VmaAllocator allocator = static_cast<VmaAllocator>(deviceObj->GetAllocator());
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    if (!allocator || !device || desc.width == 0 || desc.height == 0) return nullptr;

    const bool wantUpload = initialData != nullptr && desc.type == TextureType::Texture2D && !desc.transient &&
                            desc.mipLevels == 1 && desc.format != RHITextureFormat::Depth24 &&
                            desc.format != RHITextureFormat::Depth24Stencil8 && desc.format != RHITextureFormat::None;
    const bool wantCubeUpload = initialData != nullptr && desc.type == TextureType::TextureCube && !desc.transient &&
                                desc.mipLevels == 1 && desc.depthOrArrayLayers >= 6u &&
                                desc.format != RHITextureFormat::Depth24 &&
                                desc.format != RHITextureFormat::Depth24Stencil8 && desc.format != RHITextureFormat::None;

    VkImageType imageType = VK_IMAGE_TYPE_2D;
    if (desc.type == TextureType::Texture3D) imageType = VK_IMAGE_TYPE_3D;
    const bool isArrayLike = (desc.type == TextureType::Texture2DArray || desc.type == TextureType::TextureCube);
    uint32_t layerCount = isArrayLike ? desc.depthOrArrayLayers : 1u;
    if (desc.type == TextureType::TextureCube) {
        if (layerCount < 6u) layerCount = 6u;
        if ((layerCount % 6u) != 0u) layerCount += (6u - (layerCount % 6u));
    }

    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = imageType;
    imgInfo.format = ToVkFormat(desc.format);
    imgInfo.extent.width = desc.width;
    imgInfo.extent.height = desc.height;
    imgInfo.extent.depth = (desc.type == TextureType::Texture3D) ? desc.depthOrArrayLayers : 1;
    imgInfo.mipLevels = desc.mipLevels;
    imgInfo.arrayLayers = layerCount;
    imgInfo.samples = static_cast<VkSampleCountFlagBits>(desc.samples);
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = ToVkImageUsageForFormat(desc.format, desc.usage);
    if (wantUpload || wantCubeUpload)
        imgInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (desc.type == TextureType::TextureCube)
        imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    const VkPhysicalDevice physicalDevice = static_cast<VkPhysicalDevice>(deviceObj->GetPhysicalDevice());
    if (!ValidateImageFormatUsage(physicalDevice, imgInfo.format, imgInfo.usage)) {
        core::Log::ErrorF("CreateTextureVK: format/usage unsupported (format=%u usage=0x%x).",
                          static_cast<unsigned>(imgInfo.format), static_cast<unsigned>(imgInfo.usage));
        return nullptr;
    }

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    if (desc.transient) {
        imgInfo.usage = (desc.format == RHITextureFormat::Depth24 || desc.format == RHITextureFormat::Depth24Stencil8)
            ? (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
            : (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
        allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
    }

    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkResult res = vmaCreateImage(allocator, &imgInfo, &allocCreateInfo, &image, &allocation, nullptr);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("CreateTextureVK: vmaCreateImage failed (0x%x)", res);
        return nullptr;
    }

    if (wantUpload) {
        uint32_t srcBpp = 0, dstBpp = 0;
        if (!GetUploadLayout(desc.format, &srcBpp, &dstBpp)) {
            vmaDestroyImage(allocator, image, allocation);
            core::Log::ErrorF("CreateTextureVK: upload not implemented for format %u", static_cast<unsigned>(desc.format));
            return nullptr;
        }
        const size_t rowPitch = (static_cast<size_t>(desc.width) * dstBpp + 3u) & ~size_t{3};
        const VkDeviceSize stagingSize = static_cast<VkDeviceSize>(rowPitch * desc.height);

        VkBufferCreateInfo bufInfo = {};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = stagingSize;
        bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                 VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VkBuffer stagingBuf = VK_NULL_HANDLE;
        VmaAllocation stagingAlloc = VK_NULL_HANDLE;
        VmaAllocationInfo stagingInfo = {};
        res = vmaCreateBuffer(allocator, &bufInfo, &stagingAllocInfo, &stagingBuf, &stagingAlloc, &stagingInfo);
        if (res != VK_SUCCESS) {
            vmaDestroyImage(allocator, image, allocation);
            core::Log::ErrorF("CreateTextureVK: staging buffer failed (0x%x)", res);
            return nullptr;
        }
        void* mapped = stagingInfo.pMappedData;
        bool unmapAfter = false;
        if (!mapped) {
            if (vmaMapMemory(allocator, stagingAlloc, &mapped) != VK_SUCCESS)
                mapped = nullptr;
            else
                unmapAfter = true;
        }
        if (!mapped) {
            vmaDestroyBuffer(allocator, stagingBuf, stagingAlloc);
            vmaDestroyImage(allocator, image, allocation);
            core::Log::Error("CreateTextureVK: failed to map staging buffer");
            return nullptr;
        }
        std::memset(mapped, 0, static_cast<size_t>(stagingSize));
        CopyToStaging(desc.format, initialData, desc.width, desc.height, static_cast<uint8_t*>(mapped), rowPitch);
        vmaFlushAllocation(allocator, stagingAlloc, 0, stagingSize);
        if (unmapAfter)
            vmaUnmapMemory(allocator, stagingAlloc);

        if (!SubmitTextureUpload(deviceObj, image, imgInfo.format, desc.width, desc.height, stagingBuf)) {
            vmaDestroyBuffer(allocator, stagingBuf, stagingAlloc);
            vmaDestroyImage(allocator, image, allocation);
            core::Log::Error("CreateTextureVK: texture upload submit failed");
            return nullptr;
        }
        vmaDestroyBuffer(allocator, stagingBuf, stagingAlloc);
    }

    if (wantCubeUpload) {
        uint32_t srcBpp = 0, dstBpp = 0;
        if (!GetUploadLayout(desc.format, &srcBpp, &dstBpp)) {
            vmaDestroyImage(allocator, image, allocation);
            core::Log::ErrorF("CreateTextureVK: cube upload not implemented for format %u", static_cast<unsigned>(desc.format));
            return nullptr;
        }
        const size_t rowPitch = (static_cast<size_t>(desc.width) * dstBpp + 3u) & ~size_t{3};
        const size_t faceStagingSize = rowPitch * desc.height;
        const VkDeviceSize stagingSize = static_cast<VkDeviceSize>(faceStagingSize * 6u);
        const size_t srcFaceBytes = static_cast<size_t>(desc.width) * static_cast<size_t>(desc.height) * static_cast<size_t>(srcBpp);

        VkBufferCreateInfo bufInfo = {};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = stagingSize;
        bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                 VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VkBuffer stagingBuf = VK_NULL_HANDLE;
        VmaAllocation stagingAlloc = VK_NULL_HANDLE;
        VmaAllocationInfo stagingInfo = {};
        res = vmaCreateBuffer(allocator, &bufInfo, &stagingAllocInfo, &stagingBuf, &stagingAlloc, &stagingInfo);
        if (res != VK_SUCCESS) {
            vmaDestroyImage(allocator, image, allocation);
            core::Log::ErrorF("CreateTextureVK: cube staging buffer failed (0x%x)", res);
            return nullptr;
        }
        void* mapped = stagingInfo.pMappedData;
        bool unmapAfter = false;
        if (!mapped) {
            if (vmaMapMemory(allocator, stagingAlloc, &mapped) != VK_SUCCESS)
                mapped = nullptr;
            else
                unmapAfter = true;
        }
        if (!mapped) {
            vmaDestroyBuffer(allocator, stagingBuf, stagingAlloc);
            vmaDestroyImage(allocator, image, allocation);
            core::Log::Error("CreateTextureVK: failed to map cube staging buffer");
            return nullptr;
        }
        std::memset(mapped, 0, static_cast<size_t>(stagingSize));
        const auto* srcBase = static_cast<const uint8_t*>(initialData);
        auto* dstBase = static_cast<uint8_t*>(mapped);
        for (uint32_t face = 0; face < 6u; ++face)
            CopyToStaging(desc.format, srcBase + face * srcFaceBytes, desc.width, desc.height,
                          dstBase + face * faceStagingSize, rowPitch);
        vmaFlushAllocation(allocator, stagingAlloc, 0, stagingSize);
        if (unmapAfter)
            vmaUnmapMemory(allocator, stagingAlloc);

        if (!SubmitCubeTextureUpload(deviceObj, image, imgInfo.format, desc.width, desc.height, stagingBuf)) {
            vmaDestroyBuffer(allocator, stagingBuf, stagingAlloc);
            vmaDestroyImage(allocator, image, allocation);
            core::Log::Error("CreateTextureVK: cube texture upload submit failed");
            return nullptr;
        }
        vmaDestroyBuffer(allocator, stagingBuf, stagingAlloc);
    }

    const bool didCpuUpload = wantUpload || wantCubeUpload;
    const bool wantSrvOnlyLayoutInit =
        !didCpuUpload && !desc.sparse && !desc.transient && HasUsage(desc.usage, TextureUsage::ShaderResource) &&
        !HasUsage(desc.usage, TextureUsage::RenderTarget) && !HasUsage(desc.usage, TextureUsage::TransferDst) &&
        desc.format != RHITextureFormat::Depth24 && desc.format != RHITextureFormat::Depth24Stencil8 &&
        desc.format != RHITextureFormat::None;
    if (wantSrvOnlyLayoutInit) {
        const VkImageAspectFlags aspect = ToVkImageAspect(desc.format);
        if ((aspect & VK_IMAGE_ASPECT_COLOR_BIT) == 0u) {
            vmaDestroyImage(allocator, image, allocation);
            core::Log::Error("CreateTextureVK: SRV-only layout init skipped non-color format");
            return nullptr;
        }
        if (!SubmitImageUndefinedToShaderReadOnly(deviceObj, image, imgInfo.mipLevels, imgInfo.arrayLayers, aspect)) {
            vmaDestroyImage(allocator, image, allocation);
            core::Log::Error("CreateTextureVK: UNDEFINED -> SHADER_READ_ONLY layout init failed");
            return nullptr;
        }
    }

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = ToVkImageViewType(desc.type);
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange.aspectMask = ToVkImageAspect(desc.format);
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = imgInfo.arrayLayers;
    VkImageView imageView = VK_NULL_HANDLE;
    res = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
    if (res != VK_SUCCESS) {
        vmaDestroyImage(allocator, image, allocation);
        core::Log::ErrorF("CreateTextureVK: vkCreateImageView failed (0x%x)", res);
        return nullptr;
    }

    if (desc.debugName) {
        SetDebugNameImage(deviceObj, image, desc.debugName);
        SetDebugNameImageView(deviceObj, imageView, desc.debugName);
    }
    auto* t = new TextureVK(deviceObj);
    t->m_width = desc.width;
    t->m_height = desc.height;
    t->m_depthOrArray = layerCount;
    t->m_type = desc.type;
    t->m_format = desc.format;
    t->m_mipLevels = desc.mipLevels;
    t->m_image = image;
    t->m_imageView = imageView;
    t->m_allocation = allocation;
    if (desc.type == TextureType::Texture2DArray)
        t->m_slice2DViewsByLayer.assign(layerCount, nullptr);
    // GPU layout matches ShaderResource after upload or UNDEFINED→SRV one-shot init; keep tracker aligned for BarrierTexture / presentation.
    if (didCpuUpload || wantSrvOnlyLayoutInit) {
        BarrierTrackerVK::SetImageState(static_cast<BarrierTrackerVK*>(deviceObj->GetBarrierTracker()), image,
                                        ResourceState::ShaderResource);
    }
    return t;
}

void DestroyTextureVK(DeviceVK* deviceObj, ITexture* texture) {
    if (!deviceObj || !texture) return;
    // Route all runtime texture destruction through DeviceVK so we always use
    // the same deferred-lifetime policy (safe with in-flight command buffers).
    deviceObj->DestroyTexture(texture);
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
