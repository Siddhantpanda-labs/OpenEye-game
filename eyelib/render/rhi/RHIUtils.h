#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

/** Thin helpers built only from RHI calls. Used by pipeline and passes. */

void ClearAndSetRenderTarget(ICommandList* cmd, IRenderTarget* rt, const ClearValue& clear);
void DrawFullscreenQuad(ICommandList* cmd, IPipeline* pipeline);
void BarrierTextureForShaderRead(ICommandList* cmd, ITexture* texture);
void BarrierTextureForRenderTarget(ICommandList* cmd, ITexture* texture);

} // namespace rhi
} // namespace render
} // namespace eyelib
