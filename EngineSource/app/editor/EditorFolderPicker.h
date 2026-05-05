#pragma once

#include <string>

namespace openeye::editor {

/**
 * Show native folder picker dialog. Returns selected folder path (absolute) or empty string on cancel/failure.
 * On Windows uses IFileDialog with FOS_PICKFOLDERS.
 */
std::string ShowFolderPickerDialog();

} // namespace openeye::editor
