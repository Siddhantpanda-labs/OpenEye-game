#pragma once

namespace openeye::editor {

struct EditorContext;

namespace panels {

/** Dockable inspector panel: selected entity components (Phase 3). Pass ctx for selection. */
void ShowInspectorPanel(EditorContext* ctx);

} // namespace panels
} // namespace openeye::editor
