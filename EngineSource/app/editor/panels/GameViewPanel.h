#pragma once

namespace openeye::editor {

struct EditorContext;

namespace panels {

/** Dockable Game view: shows player runtime output when playing; placeholder otherwise. */
void ShowGameViewPanel(EditorContext* ctx);

} // namespace panels
} // namespace openeye::editor
