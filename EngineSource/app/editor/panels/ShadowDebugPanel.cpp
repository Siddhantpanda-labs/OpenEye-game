#include "ShadowDebugPanel.h"
#include "EditorContext.h"

#include "api/render/RenderPipeline.h"
#include "api/rendering/RenderPipeline.h"

#include <imgui.h>

namespace openeye::editor::panels {

void ShowShadowDebugPanel(EditorContext* ctx) {
    if (!ImGui::Begin("Shadow debug", nullptr, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    if (!ctx || (!ctx->sceneViewPipeline && !ctx->sceneViewPipelineNew)) {
        ImGui::TextUnformatted("Scene view pipeline not initialized.");
        ImGui::End();
        return;
    }

    // M3.4 cold-path branch: shadow accessors come from the new façade when wired (M3.9 flip).
    // Façade implementation lives in `api/rendering/RenderPipeline.cpp` — reads CSM array
    // depth view from the blackboard slot `DirectionalShadowMapArray`.
    uint32_t cascades = 0;
    uint32_t res = 0;
    void* rawView = nullptr;
    if (ctx->sceneViewPipelineNew) {
        cascades = ctx->sceneViewPipelineNew->GetShadowCascadeCountForDebug();
        res = ctx->sceneViewPipelineNew->GetShadowMapResolutionForDebug();
        rawView = ctx->sceneViewPipelineNew->GetShadowDepthMapNativeViewForDebug();
    } else {
        eyelib::api::RenderPipeline* pipe = ctx->sceneViewPipeline;
        cascades = pipe->GetShadowCascadeCountForDebug();
        res = pipe->GetShadowMapResolutionForDebug();
        rawView = pipe->GetShadowDepthMapNativeViewForDebug();
    }
    ImGui::Text("CSM cascades: %u   Map: %u x %u", cascades, res, res);
    ImGui::TextWrapped(
        "Depth array from the scene view pipeline (last Render). Keep the Scene view drawing for live updates. "
        "If the thumbnail is black or empty, check directional light castShadows and Vulkan depth sampling.");

    void* imguiId = rawView;
    if (rawView && ctx->resolveImGuiTextureId)
        imguiId = ctx->resolveImGuiTextureId(rawView);

    const float availW = ImGui::GetContentRegionAvail().x;
    const float side = (availW > 32.f) ? availW : 256.f;

    if (imguiId && rawView) {
        ImGui::Image(reinterpret_cast<ImTextureID>(imguiId), ImVec2(side, side), ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));
    } else {
        ImGui::TextUnformatted("No depth native view (pipeline not ready or no shadow map).");
    }

    ImGui::End();
}

} // namespace openeye::editor::panels
