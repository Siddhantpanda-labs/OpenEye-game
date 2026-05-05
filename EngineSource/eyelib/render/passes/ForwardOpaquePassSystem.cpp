#include "eyelib/render/passes/ForwardOpaquePassSystem.h"
#include "eyelib/render/rhi/RHIUtils.h"

namespace eyelib {
namespace render {

ForwardOpaquePassExecuteParams ForwardOpaquePassSystem::BuildParams(const ForwardOpaquePassBuildParams& params) const {
    ForwardOpaquePassExecuteParams out{};
    out.build = params;
    return out;
}

bool ForwardOpaquePassSystem::Execute(const ForwardOpaquePassExecuteParams& params) const {
    if (!params.build.frameData || !params.build.opaqueDrawables) return false;
    if (params.build.cmd && params.build.hdrRenderTarget)
        rhi::ClearAndSetRenderTargetWithViewport(params.build.cmd, params.build.hdrRenderTarget, 0, 0,
                                                 params.build.width, params.build.height, params.build.clearColor);
    if (params.build.cmd) {
        params.build.cmd->SetDepthTest(params.build.setDepthTest);
        params.build.cmd->SetCullFace(params.build.setCullFace);
    }
    if (params.build.drawCallback)
        params.build.drawCallback(params.build.cmd, *params.build.frameData, params.build.camView, *params.build.opaqueDrawables);
    return true;
}

}  // namespace render
}  // namespace eyelib

