#include "eyelib/render/renderer/graph/RenderGraphRecipes.h"

namespace eyelib {
namespace render {

// Scene passes only. Final post-processing (tone map, bloom, etc.) runs once in
// RenderPipeline::Render after editor overlays, entity-id, and debug draws — not here.

FrameGraphRecipe BuildDeferredRenderGraphRecipe() {
    FrameGraphRecipe recipe;
    recipe.push_back({ "GPU Cull", {}, {}, true });
    recipe.push_back({ "Shadow", {}, { "Shadow" }, true });
    recipe.push_back({ "GBuffer", {}, { "GBufferPos", "GBufferNorm", "GBufferAlb", "GBufferPBR", "GBufferVelocity", "GBufferDepth" }, true });
    recipe.push_back({ "Copy depth for occlusion", { "GBufferDepth" }, {}, true });
    recipe.push_back({ "Lighting", { "GBufferPos", "GBufferNorm", "GBufferAlb", "GBufferPBR", "Shadow" }, { "HDR" }, true });
    recipe.push_back({ "Transparency", {}, { "HDR" }, true });
    recipe.push_back({ "Skybox", {}, { "HDR" }, true });
    return recipe;
}

FrameGraphRecipe BuildForwardRenderGraphRecipe(bool includeGpuCull) {
    FrameGraphRecipe recipe;
    if (includeGpuCull)
        recipe.push_back({ "GPU Cull", {}, {}, true });
    recipe.push_back({ "Shadow", {}, { "Shadow" }, true });
    recipe.push_back({ "Forward Opaque", {}, { "HDR" }, true });
    recipe.push_back({ "Copy depth for occlusion", { "HDR" }, {}, true });
    recipe.push_back({ "Transparency", {}, { "HDR" }, true });
    recipe.push_back({ "Skybox", {}, { "HDR" }, true });
    return recipe;
}

FrameGraphRecipe BuildForwardPlusRenderGraphRecipe() {
    FrameGraphRecipe recipe;
    recipe.push_back({ "Shadow", {}, { "Shadow" }, true });
    recipe.push_back({ "Forward+ Opaque", {}, { "HDR" }, true });
    recipe.push_back({ "Transparency", {}, { "HDR" }, true });
    recipe.push_back({ "Skybox", {}, { "HDR" }, true });
    return recipe;
}

}  // namespace render
}  // namespace eyelib

