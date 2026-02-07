#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/Vulkan/vma/vk_mem_alloc.h"
#include <vulkan/vulkan_core.h>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVk;

struct TextureVk : ITexture {
    uint32_t GetWidth() const override;
    uint32_t GetHeight() const override;
    TextureType GetType() const override;
    RHITextureFormat GetFormat() const override;
    void* GetNativeTexture() override;

    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VmaAllocation m_allocation = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_depthOrArray = 1;
    TextureType m_type = TextureType::Texture2D;
    RHITextureFormat m_format = RHITextureFormat::RGBA8;
    bool m_owned = true;
    DeviceVk* m_device = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
