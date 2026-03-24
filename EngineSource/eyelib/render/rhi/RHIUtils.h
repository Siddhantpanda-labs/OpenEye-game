#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

/** Thin helpers built only from RHI calls. Used by pipeline and passes. */

void ClearAndSetRenderTarget(ICommandList* cmd, IRenderTarget* rt, const ClearValue& clear);
/** Set RT, viewport (x,y,width,height), then clear. Use for full-rt setup. */
void ClearAndSetRenderTargetWithViewport(ICommandList* cmd, IRenderTarget* rt,
                                         uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                         const ClearValue& clear);
void DrawFullscreenQuad(ICommandList* cmd, IPipeline* pipeline);
void BarrierTextureForShaderRead(ICommandList* cmd, ITexture* texture);
void BarrierTextureForRenderTarget(ICommandList* cmd, ITexture* texture);
/**
 * After rendering to a depth/stencil image as an attachment, call before it is bound for shader sampling
 * (e.g. shadow map). Vulkan: matches descriptor layout (SHADER_READ_ONLY_OPTIMAL) vs attachment layout.
 */
void BarrierDepthTextureForShaderSampling(ICommandList* cmd, ITexture* depthTexture);
/** Transition indirect buffer from ShaderResource (after compute write) to IndirectDraw for Draw*Indirect. Call before MultiDrawIndexedIndirect etc. */
void BarrierBufferForIndirectDraw(ICommandList* cmd, IBuffer* buffer);

} // namespace rhi
} // namespace render
} // namespace eyelib
