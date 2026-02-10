#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

struct TextureBGFX : ITexture {
    ~TextureBGFX() override;
    uint32_t GetWidth() const override;
    uint32_t GetHeight() const override;
    TextureType GetType() const override;
    RHITextureFormat GetFormat() const override;
    void* GetNativeTexture() override;

    static constexpr uint16_t kInvalidIdx = 0xffff;
    uint16_t m_texIdx = kInvalidIdx;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_depthOrArray = 1;
    TextureType m_type = TextureType::Texture2D;
    RHITextureFormat m_format = RHITextureFormat::RGBA8;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
