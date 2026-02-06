#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct ShaderGL : IShader {
    ShaderStage GetStage() const override;
    bool IsValid() const override;

    unsigned int m_handle = 0;  // GL shader object
    ShaderStage m_stage = ShaderStage::Vertex;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
