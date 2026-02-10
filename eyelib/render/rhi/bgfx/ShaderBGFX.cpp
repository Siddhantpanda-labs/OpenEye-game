#include "eyelib/render/rhi/bgfx/ShaderBGFX.h"
#include <bgfx/bgfx.h>

namespace eyelib {
namespace render {
namespace rhi {

ShaderBGFX::~ShaderBGFX() = default;

ShaderStage ShaderBGFX::GetStage() const { return m_stage; }

bool ShaderBGFX::IsValid() const {
    if (m_shaderIdx == kInvalidIdx) return false;
    bgfx::ShaderHandle h; h.idx = m_shaderIdx;
    return bgfx::isValid(h);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
