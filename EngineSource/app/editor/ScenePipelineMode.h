#pragma once

#include "EditorContext.h"
#include "api/render/RenderPipeline.h"
#include "api/rendering/RenderPipeline.h"

namespace openeye::editor {

inline eyelib::api::RenderPipeline::Mode SceneModeStringToPipelineMode(const std::string& mode) {
    if (mode == "Hybrid" || mode == "Deferred") return eyelib::api::RenderPipeline::Mode::Hybrid;
    if (mode == "ForwardPlus") return eyelib::api::RenderPipeline::Mode::ForwardPlus;
    return eyelib::api::RenderPipeline::Mode::Forward;
}

inline const char* PipelineModeToSceneModeString(eyelib::api::RenderPipeline::Mode mode) {
    switch (mode) {
    case eyelib::api::RenderPipeline::Mode::Hybrid: return "Hybrid";
    case eyelib::api::RenderPipeline::Mode::ForwardPlus: return "ForwardPlus";
    case eyelib::api::RenderPipeline::Mode::Forward:
    default:
        return "Forward";
    }
}

/**
 * M3.3 — translation between legacy mode names (the editor / scene JSON's source of truth)
 * and the new façade's `eyelib::api::rendering::RenderPipeline::Mode { Forward, Deferred,
 * Hybrid, ForwardPlus }`. Mapping mirrors the M1.4 façade decisions:
 *  - "Hybrid" / "Deferred" → `Mode::Hybrid` (façade internally remaps to deferred raster).
 *  - "ForwardPlus"        → `Mode::ForwardPlus` (façade falls back to Forward + warns).
 *  - default              → `Mode::Forward`.
 *
 * Keeps the inspector dropdown / scene JSON working unchanged for both renderers.
 */
inline eyelib::api::rendering::RenderPipeline::Mode SceneModeStringToPipelineModeNew(const std::string& mode) {
    if (mode == "Hybrid" || mode == "Deferred") return eyelib::api::rendering::RenderPipeline::Mode::Hybrid;
    if (mode == "ForwardPlus") return eyelib::api::rendering::RenderPipeline::Mode::ForwardPlus;
    return eyelib::api::rendering::RenderPipeline::Mode::Forward;
}

inline void ApplyScenePipelineModeToEditorPipelines(EditorContext& ctx) {
    const std::string& modeStr = ctx.world.GetRenderPipelineMode();
    const eyelib::api::RenderPipeline::Mode mode = SceneModeStringToPipelineMode(modeStr);
    if (ctx.sceneViewPipeline) ctx.sceneViewPipeline->SetMode(mode);
    if (ctx.gameViewPipeline) ctx.gameViewPipeline->SetMode(mode);
    // Material preview pipeline stays Forward-only (see EditorApp + MaterialPreview); do not mirror Hybrid here.

    // M3.3 fan-out to the new pipelines (cold during M2/M3 — the `*New` pointers stay null
    // until the M3.9 device flip, so this is a no-op until then).
    const eyelib::api::rendering::RenderPipeline::Mode modeNew = SceneModeStringToPipelineModeNew(modeStr);
    if (ctx.sceneViewPipelineNew) ctx.sceneViewPipelineNew->SetMode(modeNew);
    if (ctx.gameViewPipelineNew) ctx.gameViewPipelineNew->SetMode(modeNew);
    // Material preview (new) stays Forward-only — see MaterialPreview cold branch.
}

/** Queue pipeline mode sync for the next GPU frame boundary (before BeginFrame). Safe from UI callbacks during recording. */
inline void RequestApplyScenePipelineModeToEditorPipelines(EditorContext& ctx) {
    ctx.EnqueueGpuSafeWork([&ctx]() { ApplyScenePipelineModeToEditorPipelines(ctx); });
}

inline void SetScenePipelineMode(EditorContext& ctx, eyelib::api::RenderPipeline::Mode mode) {
    (void)ctx.world.SetRenderPipelineMode(PipelineModeToSceneModeString(mode));
    RequestApplyScenePipelineModeToEditorPipelines(ctx);
}

} // namespace openeye::editor
