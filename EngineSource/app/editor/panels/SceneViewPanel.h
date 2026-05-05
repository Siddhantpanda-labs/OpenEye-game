#pragma once

namespace openeye::editor {

struct EditorContext;

namespace panels {

/** Dockable scene view panel: viewport (Phase 4). Uses context.editorCamera; placeholder until render-to-texture is wired. */
void ShowSceneViewPanel(EditorContext* ctx);

} // namespace panels
} // namespace openeye::editor
