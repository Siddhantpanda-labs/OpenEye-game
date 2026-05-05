#include "ShadowDebugPanel.h"
#include "EditorContext.h"

#include "api/render/RenderPipeline.h"

#include <imgui.h>

namespace openeye::editor::panels {

void ShowShadowDebugPanel(EditorContext* ctx) {
    if (!ImGui::Begin("Shadow debug", nullptr, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    if (!ctx || !ctx->sceneViewPipeline) {
        ImGui::TextUnformatted("Scene view pipeline not initialized.");
        ImGui::End();
        return;
    }

    eyelib::api::RenderPipeline* pipe = ctx->sceneViewPipeline;
    const uint32_t cascades = pipe->GetShadowCascadeCountForDebug();
    const uint32_t res = pipe->GetShadowMapResolutionForDebug();
    ImGui::Text("CSM cascades: %u   Map: %u x %u", cascades, res, res);
    ImGui::TextWrapped(
        "Depth array from the scene view pipeline (last Render). Keep the Scene view drawing for live updates. "
        "If the thumbnail is black or empty, check directional light castShadows and Vulkan depth sampling.");

    void* rawView = pipe->GetShadowDepthMapNativeViewForDebug();
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
