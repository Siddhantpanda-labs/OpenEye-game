#pragma once

namespace openeye::editor {

struct EditorContext;

/** Draw main menu bar (File → Open, Save, Save As) and path modals. Uses context.world for load/save. */
void ShowMenuBar(EditorContext& ctx);

} // namespace openeye::editor
