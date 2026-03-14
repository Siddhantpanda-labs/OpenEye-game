#pragma once

#include "EditorContext.h"
#include "api/render/RenderPipeline.h"

namespace openeye::editor {

inline eyelib::api::RenderPipeline::Mode SceneModeStringToPipelineMode(const std::string& mode) {
    if (mode == "Deferred") return eyelib::api::RenderPipeline::Mode::Deferred;
    if (mode == "ForwardPlus") return eyelib::api::RenderPipeline::Mode::ForwardPlus;
    return eyelib::api::RenderPipeline::Mode::Forward;
}

inline const char* PipelineModeToSceneModeString(eyelib::api::RenderPipeline::Mode mode) {
    switch (mode) {
    case eyelib::api::RenderPipeline::Mode::Deferred: return "Deferred";
    case eyelib::api::RenderPipeline::Mode::ForwardPlus: return "ForwardPlus";
    case eyelib::api::RenderPipeline::Mode::Forward:
    default:
        return "Forward";
    }
}

inline void ApplyScenePipelineModeToEditorPipelines(EditorContext& ctx) {
    const eyelib::api::RenderPipeline::Mode mode = SceneModeStringToPipelineMode(ctx.world.GetRenderPipelineMode());
    if (ctx.sceneViewPipeline) ctx.sceneViewPipeline->SetMode(mode);
    if (ctx.gameViewPipeline) ctx.gameViewPipeline->SetMode(mode);
}

inline void SetScenePipelineMode(EditorContext& ctx, eyelib::api::RenderPipeline::Mode mode) {
    (void)ctx.world.SetRenderPipelineMode(PipelineModeToSceneModeString(mode));
    ApplyScenePipelineModeToEditorPipelines(ctx);
}

} // namespace openeye::editor
