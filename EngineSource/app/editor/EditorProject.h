#pragma once

#include "EditorContext.h"
#include <string>

namespace openeye::editor {

/** Open a project by path (folder containing a .openeye file). Sets ctx project fields and assetRoots. Returns true on success. */
bool OpenProject(EditorContext& ctx, const std::string& projectFolderPath);

/** Clear project fields and set assetRoots to engine-only (caller must pass engine assets path). */
void CloseProject(EditorContext& ctx, const std::string& engineAssetsPath);

} // namespace openeye::editor
