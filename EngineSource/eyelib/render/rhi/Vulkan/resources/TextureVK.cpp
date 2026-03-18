#include "eyelib/render/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/render/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/render/rhi/Vulkan/memory/vk_mem_alloc.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

TextureVK::TextureVK(DeviceVK* device) : m_device(device) {}

TextureVK::~TextureVK() = default;

uint32_t TextureVK::GetWidth() const { return m_width; }

uint32_t TextureVK::GetHeight() const { return m_height; }

TextureType TextureVK::GetType() const { return m_type; }

RHITextureFormat TextureVK::GetFormat() const { return m_format; }

void* TextureVK::GetNativeTexture() { return m_imageView; }

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

bool SubmitTextureUpload(DeviceVK* deviceObj, VkImage image, VkFormat vkFormat, uint32_t width, uint32_t height, VkBuffer staging) {
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    VkQueue queue = static_cast<VkQueue>(deviceObj->GetGraphicsQueue());
    if (!queue) return false;

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = deviceObj->GetGraphicsQueueFamily();
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

    VkImageMemoryBarrier toTransfer = {};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
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
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

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

    VkImageMemoryBarrier toSample = {};
    toSample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toSample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toSample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.image = image;
    toSample.subresourceRange = toTransfer.subresourceRange;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSample);

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

} // namespace

ITexture* CreateTextureVK(DeviceVK* deviceObj, const TextureDesc& desc, const void* initialData) {
    if (!deviceObj) return nullptr;
    VmaAllocator allocator = static_cast<VmaAllocator>(deviceObj->GetAllocator());
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    if (!allocator || !device || desc.width == 0 || desc.height == 0) return nullptr;

    const bool wantUpload = initialData != nullptr && desc.type == TextureType::Texture2D && !desc.transient &&
                            desc.mipLevels == 1 && desc.format != RHITextureFormat::Depth24 &&
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
    if (wantUpload)
        imgInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (desc.type == TextureType::TextureCube)
        imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

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
    t->m_image = image;
    t->m_imageView = imageView;
    t->m_allocation = allocation;
    return t;
}

void DestroyTextureVK(DeviceVK* deviceObj, ITexture* texture) {
    if (!deviceObj || !texture) return;
    TextureVK* t = static_cast<TextureVK*>(texture);
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    if (device && t->m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, static_cast<VkImageView>(t->m_imageView), nullptr);
        t->m_imageView = nullptr;
    }
    VmaAllocator allocator = static_cast<VmaAllocator>(deviceObj->GetAllocator());
    if (allocator && t->m_image != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, static_cast<VkImage>(t->m_image), static_cast<VmaAllocation>(t->m_allocation));
        t->m_image = nullptr;
        t->m_allocation = nullptr;
    }
    delete t;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
