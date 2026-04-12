#pragma once

namespace openeye::editor {

struct EditorContext;

namespace panels {

/** ImGui window: directional CSM depth (scene view pipeline). Dock or close from title bar. */
void ShowShadowDebugPanel(EditorContext* ctx);

} // namespace panels
} // namespace openeye::editor
