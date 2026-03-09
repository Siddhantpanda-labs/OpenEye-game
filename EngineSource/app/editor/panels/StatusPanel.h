#pragma once

namespace openeye::editor {

struct EditorContext;

namespace panels {

/** Dockable status / welcome panel. Pass context for scene path and entity count. */
void ShowStatusPanel(EditorContext* ctx);

} // namespace panels
} // namespace openeye::editor
