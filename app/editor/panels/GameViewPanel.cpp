#include "GameViewPanel.h"
#include "EditorContext.h"
#include "GameRuntime.h"

#include <imgui.h>

namespace openeye::editor::panels {

void ShowGameViewPanel(EditorContext* ctx) {
    if (!ctx) return;

    const char* title = "Game";
    ImGui::Begin(title);

    if (ctx->isPlaying && ctx->gameRuntime && ctx->gameRuntime->IsReady()) {
        float deltaSeconds = ImGui::GetIO().DeltaTime;
        if (deltaSeconds <= 0.f) deltaSeconds = 1.f / 60.f;
        ctx->gameRuntime->Tick(deltaSeconds);

        ImVec2 region = ImGui::GetContentRegionAvail();
        uint32_t w = (region.x > 1.f) ? static_cast<uint32_t>(region.x) : 1u;
        uint32_t h = (region.y > 1.f) ? static_cast<uint32_t>(region.y) : 1u;
        (void)ctx->gameRuntime->Render(w, h);

        void* texId = ctx->gameRuntime->GetOutputColorTextureNativeHandle();
        if (texId) {
            ImVec2 size(static_cast<float>(w), static_cast<float>(h));
            ImGui::Image(texId, size, ImVec2(0, 1), ImVec2(1, 0));
        } else {
            ImGui::TextUnformatted("No output");
        }
    } else {
        ImGui::TextUnformatted("Press Play to run the game.");
    }

    ImGui::End();
}

} // namespace openeye::editor::panels
