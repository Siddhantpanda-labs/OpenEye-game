#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

struct TextureVK : ITexture {
    explicit TextureVK(DeviceVK* device);
    ~TextureVK() override;

    uint32_t GetWidth() const override;
    uint32_t GetHeight() const override;
    TextureType GetType() const override;
    RHITextureFormat GetFormat() const override;
    void* GetNativeTexture() override;
    bool IsGpuBindable() const override;

    DeviceVK* m_device = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_depthOrArray = 1;
    TextureType m_type = TextureType::Texture2D;
    RHITextureFormat m_format = RHITextureFormat::RGBA8;
    bool m_owned = true;
    // True after DestroyTexture schedules this wrapper for deferred retire.
    bool m_retired = false;
    // VkImage + VkImageView + VmaAllocation
    void* m_image = nullptr;
    void* m_imageView = nullptr;
    void* m_allocation = nullptr;
};

ITexture* CreateTextureVK(DeviceVK* device, const TextureDesc& desc, const void* initialData);
void DestroyTextureVK(DeviceVK* device, ITexture* texture);

} // namespace rhi
} // namespace render
} // namespace eyelib
