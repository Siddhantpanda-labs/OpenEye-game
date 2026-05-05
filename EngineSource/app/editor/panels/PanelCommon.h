#pragma once

/**
 * @file PanelCommon.h
 * @brief Convention for editor panels: each panel provides a Show() function.
 *
 * All panels are dockable: use ImGui::Begin with default flags (no NoDocking)
 * so windows can be dragged into the main dockspace. Keep window names unique.
 */

#include <imgui.h>

namespace openeye::editor::panels {

// Each panel implements:
//   void Show();
// and uses ImGui::Begin("PanelName", ...) with flags that allow docking (default).

} // namespace openeye::editor::panels
