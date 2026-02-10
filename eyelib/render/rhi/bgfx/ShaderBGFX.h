#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

struct ShaderBGFX : IShader {
    ~ShaderBGFX() override;
    ShaderStage GetStage() const override;
    bool IsValid() const override;

    static constexpr uint16_t kInvalidIdx = 0xffff;
    uint16_t m_shaderIdx = kInvalidIdx;
    ShaderStage m_stage = ShaderStage::Vertex;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
