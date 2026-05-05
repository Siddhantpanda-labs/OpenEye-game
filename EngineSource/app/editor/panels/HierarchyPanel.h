#pragma once

namespace openeye::editor {

struct EditorContext;

namespace panels {

/** Dockable hierarchy panel: entity tree, selection, Add Entity/Add Child/Remove. Rename is in Inspector. */
void ShowHierarchyPanel(EditorContext* ctx);

} // namespace panels
} // namespace openeye::editor
