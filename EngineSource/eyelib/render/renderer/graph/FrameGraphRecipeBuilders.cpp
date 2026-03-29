#include "eyelib/render/renderer/graph/FrameGraphRecipeBuilders.h"
#include "eyelib/render/renderer/graph/RenderGraphRecipes.h"

namespace eyelib {
namespace render {

void WireDeferredFrameGraph(FrameGraph& fg, const DeferredFrameGraphWiringInput& in,
                            const std::function<FrameGraph::ExecuteFunc(const std::string&)>& getExecutor) {
    FGNameToHandle nameToHandle;
    nameToHandle["HDR"] = fg.ImportTextureRHI("HDR", in.hdrColor, in.width, in.height);
    nameToHandle["HDRDepth"] = fg.ImportTextureRHI("HDRDepth", in.hdrDepth, in.width, in.height);
    nameToHandle["GBufferPos"] = fg.ImportTextureRHI("GBufferPos", in.gbufferPos, in.width, in.height);
    nameToHandle["GBufferNorm"] = fg.ImportTextureRHI("GBufferNorm", in.gbufferNorm, in.width, in.height);
    nameToHandle["GBufferAlb"] = fg.ImportTextureRHI("GBufferAlb", in.gbufferAlb, in.width, in.height);
    nameToHandle["GBufferPBR"] = fg.ImportTextureRHI("GBufferPBR", in.gbufferPbr, in.width, in.height);
    nameToHandle["GBufferVelocity"] = fg.ImportTextureRHI("GBufferVelocity", in.gbufferVelocity, in.width, in.height);
    nameToHandle["GBufferDepth"] = fg.ImportTextureRHI("GBufferDepth", in.gbufferDepth, in.width, in.height);
    nameToHandle["Shadow"] =
        fg.ImportTextureRHI("Shadow", in.shadowDepth, in.shadowWidth, in.shadowHeight);

    const FrameGraphRecipe recipe = BuildDeferredRenderGraphRecipe();
    AddPassesFromRecipe(fg, recipe, nameToHandle, getExecutor);
}

void WireForwardFrameGraph(FrameGraph& fg, const ForwardFrameGraphWiringInput& in, bool includeGpuCull,
                           const std::function<FrameGraph::ExecuteFunc(const std::string&)>& getExecutor) {
    FGNameToHandle nameToHandle;
    nameToHandle["HDR"] = fg.ImportTextureRHI("HDR", in.hdrColor, in.width, in.height);
    nameToHandle["Shadow"] =
        fg.ImportTextureRHI("Shadow", in.shadowDepth, in.shadowWidth, in.shadowHeight);

    const FrameGraphRecipe recipe = BuildForwardRenderGraphRecipe(includeGpuCull);
    AddPassesFromRecipe(fg, recipe, nameToHandle, getExecutor);
}

void WireForwardPlusFrameGraph(FrameGraph& fg, const ForwardFrameGraphWiringInput& in,
                               const std::function<FrameGraph::ExecuteFunc(const std::string&)>& getExecutor) {
    FGNameToHandle nameToHandle;
    nameToHandle["HDR"] = fg.ImportTextureRHI("HDR", in.hdrColor, in.width, in.height);
    nameToHandle["Shadow"] =
        fg.ImportTextureRHI("Shadow", in.shadowDepth, in.shadowWidth, in.shadowHeight);

    const FrameGraphRecipe recipe = BuildForwardPlusRenderGraphRecipe();
    AddPassesFromRecipe(fg, recipe, nameToHandle, getExecutor);
}

void WireForwardStereoFrameGraph(FrameGraph& fg, const ForwardFrameGraphWiringInput& in,
                                 const std::function<FrameGraph::ExecuteFunc(const std::string&)>& getExecutor,
                                 const FrameGraph::ExecuteFunc& skyboxLeft,
                                 const FrameGraph::ExecuteFunc& skyboxRight) {
    FGNameToHandle nameToHandle;
    nameToHandle["HDR"] = fg.ImportTextureRHI("HDR", in.hdrColor, in.width, in.height);
    nameToHandle["Shadow"] =
        fg.ImportTextureRHI("Shadow", in.shadowDepth, in.shadowWidth, in.shadowHeight);

    const FrameGraphRecipe recipe = BuildForwardStereoRenderGraphRecipe();
    AddPassesFromRecipe(fg, recipe, nameToHandle, getExecutor);
    fg.AddPass("SkyboxLeft", skyboxLeft, {}, { nameToHandle["HDR"] }, true);
    fg.AddPass("SkyboxRight", skyboxRight, {}, { nameToHandle["HDR"] }, true);
}

}  // namespace render
}  // namespace eyelib
