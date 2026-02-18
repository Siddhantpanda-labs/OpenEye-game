#include "eyelib/render/rhi/RHIUtils.h"

namespace eyelib {
namespace render {
namespace rhi {

void ClearAndSetRenderTarget(ICommandList* cmd, IRenderTarget* rt, const ClearValue& clear) {
    if (!cmd || !rt) return;
    cmd->SetRenderTarget(rt);
    cmd->ClearRenderTarget(rt, clear);
}

void ClearAndSetRenderTargetWithViewport(ICommandList* cmd, IRenderTarget* rt,
                                         uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                         const ClearValue& clear) {
    if (!cmd || !rt) return;
    cmd->SetRenderTarget(rt);
    cmd->SetViewport(x, y, width, height);
    cmd->ClearRenderTarget(rt, clear);
}

void DrawFullscreenQuad(ICommandList* cmd, IPipeline* pipeline) {
    if (!cmd || !pipeline) return;
    cmd->DrawFullscreenQuad(pipeline);
}

void BarrierTextureForShaderRead(ICommandList* cmd, ITexture* texture) {
    if (!cmd || !texture) return;
    cmd->BarrierTexture(texture, ResourceState::RenderTarget, ResourceState::ShaderResource);
}

void BarrierTextureForRenderTarget(ICommandList* cmd, ITexture* texture) {
    if (!cmd || !texture) return;
    cmd->BarrierTexture(texture, ResourceState::ShaderResource, ResourceState::RenderTarget);
}

void BarrierBufferForIndirectDraw(ICommandList* cmd, IBuffer* buffer) {
    if (!cmd || !buffer) return;
    cmd->BarrierBuffer(buffer, ResourceState::ShaderResource, ResourceState::IndirectDraw);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
