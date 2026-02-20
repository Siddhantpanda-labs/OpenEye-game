#include "SceneViewPanel.h"
#include "PanelCommon.h"

namespace openeye::editor::panels {

void ShowSceneViewPanel() {
    if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_None)) {
        ImGui::TextUnformatted("(Viewport — Phase 4)");
    }
    ImGui::End();
}

} // namespace openeye::editor::panels
