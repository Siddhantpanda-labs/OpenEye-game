#include "eyelib/render/renderer/graph/RenderGraphRecipes.h"
#include "eyelib/render/renderer/graph/FrameGraphPassNames.h"

namespace eyelib {
namespace render {

namespace P = FrameGraphPassNames;

// Scene passes only. Final post-processing (tone map, bloom, etc.) runs once in
// RenderPipeline::Render after editor overlays, entity-id, and debug draws — not here.

FrameGraphRecipe BuildDeferredRenderGraphRecipe() {
    FrameGraphRecipe recipe;
    recipe.push_back({ P::kGpuCull, {}, {}, true });
    recipe.push_back({ P::kShadow, {}, { "Shadow" }, true });
    recipe.push_back({ P::kGBuffer, {}, { "GBufferPos", "GBufferNorm", "GBufferAlb", "GBufferPBR", "GBufferVelocity", "GBufferDepth" }, true });
    recipe.push_back({ P::kCopyDepthOcclusion, { "GBufferDepth" }, {}, true });
    recipe.push_back({ P::kLighting, { "GBufferPos", "GBufferNorm", "GBufferAlb", "GBufferPBR", "Shadow" }, { "HDR" }, true });
    recipe.push_back({ P::kTransparency, {}, { "HDR" }, true });
    recipe.push_back({ P::kSkybox, {}, { "HDR" }, true });
    return recipe;
}

FrameGraphRecipe BuildForwardRenderGraphRecipe(bool includeGpuCull) {
    FrameGraphRecipe recipe;
    if (includeGpuCull)
        recipe.push_back({ P::kGpuCull, {}, {}, true });
    recipe.push_back({ P::kShadow, {}, { "Shadow" }, true });
    recipe.push_back({ P::kForwardOpaque, {}, { "HDR" }, true });
    recipe.push_back({ P::kCopyDepthOcclusion, { "HDR" }, {}, true });
    recipe.push_back({ P::kTransparency, {}, { "HDR" }, true });
    recipe.push_back({ P::kSkybox, {}, { "HDR" }, true });
    return recipe;
}

FrameGraphRecipe BuildForwardPlusRenderGraphRecipe() {
    FrameGraphRecipe recipe;
    recipe.push_back({ P::kShadow, {}, { "Shadow" }, true });
    recipe.push_back({ P::kForwardPlusOpaque, {}, { "HDR" }, true });
    recipe.push_back({ P::kTransparency, {}, { "HDR" }, true });
    recipe.push_back({ P::kSkybox, {}, { "HDR" }, true });
    return recipe;
}

FrameGraphRecipe BuildForwardStereoRenderGraphRecipe() {
    FrameGraphRecipe recipe;
    recipe.push_back({ P::kShadow, {}, { "Shadow" }, true });
    recipe.push_back({ P::kForwardOpaque, {}, { "HDR" }, true });
    recipe.push_back({ P::kTransparency, {}, { "HDR" }, true });
    return recipe;
}

}  // namespace render
}  // namespace eyelib
