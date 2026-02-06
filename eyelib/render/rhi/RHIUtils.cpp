#include "eyelib/render/rhi/RHIUtils.h"

namespace eyelib {
namespace render {
namespace rhi {

void ClearAndSetRenderTarget(ICommandList* cmd, IRenderTarget* rt, const ClearValue& clear) {
    if (!cmd || !rt) return;
    cmd->SetRenderTarget(rt);
    cmd->ClearRenderTarget(rt, clear);
}

void DrawFullscreenQuad(ICommandList* cmd, IPipeline* pipeline) {
    if (!cmd || !pipeline) return;
    cmd->BindPipeline(pipeline);
    cmd->Draw(3, 0);  // triangle (vertex ID 0,1,2 in vertex shader)
}

void BarrierTextureForShaderRead(ICommandList* cmd, ITexture* texture) {
    if (!cmd || !texture) return;
    cmd->BarrierTexture(texture, ResourceState::RenderTarget, ResourceState::ShaderResource);
}

void BarrierTextureForRenderTarget(ICommandList* cmd, ITexture* texture) {
    if (!cmd || !texture) return;
    cmd->BarrierTexture(texture, ResourceState::ShaderResource, ResourceState::RenderTarget);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
