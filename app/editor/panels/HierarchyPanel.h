#pragma once

namespace openeye::editor {

struct EditorContext;

namespace panels {

/** Dockable hierarchy panel: entity tree, selection, Create/Delete/Rename (Phase 2). */
void ShowHierarchyPanel(EditorContext* ctx);

} // namespace panels
} // namespace openeye::editor
