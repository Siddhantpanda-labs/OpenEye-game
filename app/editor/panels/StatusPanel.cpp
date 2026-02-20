#include "StatusPanel.h"
#include "EditorContext.h"
#include "PanelCommon.h"

namespace openeye::editor::panels {

void ShowStatusPanel(EditorContext* ctx) {
    if (ImGui::Begin("Status", nullptr, ImGuiWindowFlags_None)) {
        ImGui::Text("OpenEye Editor — Phase 1 (Load/Save)");
        ImGui::Separator();
        if (ctx) {
            ImGui::Text("Scene: %s", ctx->scenePath.empty() ? "(none)" : ctx->scenePath.c_str());
            ImGui::Text("Entities: %zu", ctx->world.GetEntityCount());
        } else {
            ImGui::TextUnformatted("All panels are dockable. File → Open/Save/Save As.");
        }
    }
    ImGui::End();
}

} // namespace openeye::editor::panels
