#include "eyelib/render/passes/TransparencyPassSystem.h"

namespace eyelib {
namespace render {

TransparencyPassExecuteParams TransparencyPassSystem::BuildParams(const TransparencyPassBuildParams& params) const {
    TransparencyPassExecuteParams out{};
    out.build = params;
    return out;
}

bool TransparencyPassSystem::Execute(const TransparencyPassExecuteParams& params) const {
    if (!params.build.frameData) return false;
    if (params.build.copyDepthFromGBuffer && params.build.device && params.build.hdrRenderTarget && params.build.gbufferRenderTarget)
        params.build.device->BlitDepthOnly(params.build.gbufferRenderTarget, params.build.hdrRenderTarget);
    if (params.build.cmd && params.build.hdrRenderTarget) {
        params.build.cmd->SetRenderTarget(params.build.hdrRenderTarget);
        params.build.cmd->SetViewport(0, 0, params.build.width, params.build.height);
    }
    if (params.build.preferVulkanPath) {
        if (params.build.drawVulkan) params.build.drawVulkan(params.build.cmd, *params.build.frameData, params.build.camView);
    } else {
        if (params.build.drawLegacy)
            params.build.drawLegacy(params.build.device, params.build.cmd, params.build.frameData->visibleTransparent,
                                    params.build.camView, params.build.shaderCache);
    }
    if (params.build.cmd) params.build.cmd->SetRenderTarget(nullptr);
    return true;
}

}  // namespace render
}  // namespace eyelib

