#include "eyelib/render/passes/PostProcessPassSystem.h"
#include "eyelib/render/renderer/GBuffer.h"

namespace eyelib {
namespace render {

PostProcessPassExecuteParams PostProcessPassSystem::BuildParams(const PostProcessPassBuildParams& params) const {
    PostProcessPassExecuteParams out{};
    out.postProcessor = params.postProcessor;
    out.gbuffer = params.gbuffer;
    out.hdrTex = params.hdrTex;
    out.skyboxTex = params.skyboxTex;
    out.probeTex = params.probeTex;
    out.camView = params.camView;
    out.invView = params.invView;
    out.invProj = params.invProj;
    out.time = params.time;
    out.settings.ssrNearPlane = params.ssrNearPlane;
    out.settings.ssrFarPlane = params.ssrFarPlane;
    return out;
}

bool PostProcessPassSystem::Execute(const PostProcessPassExecuteParams& params) const {
    if (!params.postProcessor || !params.gbuffer) return false;
    (void)params.postProcessor->ApplyPostProcessing(
        params.hdrTex, *params.gbuffer, params.settings,
        params.camView.view, params.camView.projection,
        params.invView, params.invProj, params.camView.viewPos, params.skyboxTex, params.time, params.probeTex);
    return true;
}

}  // namespace render
}  // namespace eyelib

