#pragma once

namespace openeye::editor {

/**
 * @brief Runs the OpenEye Editor (Phase 0: bootstrap).
 * @return Exit code: 0 on success, non-zero on failure.
 *
 * Uses only api/ (core, platform, ImGui). No eyelib/ includes.
 */
int RunEditor(int argc, char** argv);

} // namespace openeye::editor
