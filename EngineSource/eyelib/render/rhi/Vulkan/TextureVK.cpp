#include "eyelib/render/rhi/Vulkan/TextureVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/VulkanEnums.h"
#include "eyelib/render/rhi/Vulkan/vk_mem_alloc.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>

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

ITexture* CreateTextureVK(DeviceVK* deviceObj, const TextureDesc& desc, const void* initialData) {
    (void)initialData;
    if (!deviceObj) return nullptr;
    VmaAllocator allocator = static_cast<VmaAllocator>(deviceObj->GetAllocator());
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    if (!allocator || !device || desc.width == 0 || desc.height == 0) return nullptr;

    VkImageType imageType = VK_IMAGE_TYPE_2D;
    if (desc.type == TextureType::Texture3D) imageType = VK_IMAGE_TYPE_3D;
    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = imageType;
    imgInfo.format = ToVkFormat(desc.format);
    imgInfo.extent.width = desc.width;
    imgInfo.extent.height = desc.height;
    imgInfo.extent.depth = (desc.type == TextureType::Texture3D) ? desc.depthOrArrayLayers : 1;
    imgInfo.mipLevels = desc.mipLevels;
    imgInfo.arrayLayers = (desc.type == TextureType::Texture2DArray || desc.type == TextureType::TextureCube) ? desc.depthOrArrayLayers : 1;
    imgInfo.samples = static_cast<VkSampleCountFlagBits>(desc.samples);
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = ToVkImageUsageForFormat(desc.format, desc.usage);
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

    auto* t = new TextureVK(deviceObj);
    t->m_width = desc.width;
    t->m_height = desc.height;
    t->m_depthOrArray = desc.depthOrArrayLayers;
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
