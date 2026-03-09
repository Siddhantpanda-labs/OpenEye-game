#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

struct ShaderDiligent : IShader {
    ~ShaderDiligent() override;
    ShaderStage GetStage() const override;
    bool IsValid() const override;

    ShaderStage m_stage = ShaderStage::Vertex;
    bool m_valid = false;
    /** When non-null, holds Diligent::IShader* (owned; Release in dtor). Used when created from HLSL. */
    void* m_pNative = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
