#pragma once

#include "eyelib/rendering/rhi/RHI.h"
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

/** 2D/cube `VkImage` + main `VkImageView`; optional per-array-layer 2D views for interop. `RHITextureFormat` maps through
 *  `ToVkFormat` / aspect helpers (`VulkanEnums.h`). Layout and barriers: internal `vkCmdPipelineBarrier` during upload and
 *  `BarrierTrackerVK` when bound through the render path; **`m_retired`** is set when destroy is deferred to the deletion queue. */
struct TextureVK : ITexture {
    explicit TextureVK(DeviceVK* device);
    ~TextureVK() override;

    uint32_t GetWidth() const override;
    uint32_t GetHeight() const override;
    TextureType GetType() const override;
    RHITextureFormat GetFormat() const override;
    void* GetNativeTexture() override;
    void* GetNativeTexture2DViewForArrayLayer(uint32_t layer) override;
    bool IsGpuBindable() const override;

    DeviceVK* m_device = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_depthOrArray = 1;
    TextureType m_type = TextureType::Texture2D;
    RHITextureFormat m_format = RHITextureFormat::RGBA8;
    uint32_t m_mipLevels = 1;
    bool m_owned = true;
    // True after DestroyTexture schedules this wrapper for deferred retire.
    bool m_retired = false;
    void* m_image = nullptr;       // VkImage
    void* m_imageView = nullptr;
    void* m_allocation = nullptr;
    /** Lazily created VK_IMAGE_VIEW_TYPE_2D views (one slot per array layer); used for ImGui sampler2D interop. */
    std::vector<void*> m_slice2DViewsByLayer;
};

ITexture* CreateTextureVK(DeviceVK* device, const TextureDesc& desc, const void* initialData);
void DestroyTextureVK(DeviceVK* device, ITexture* texture);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
