#pragma once

/**
 * @brief Filesystem API. Re-exports eyelib::platform::filesystem.
 */
#include "eyelib/platform/filesystem/FileSystem.h"

namespace api {
namespace platform {
namespace filesystem {

using eyelib::platform::filesystem::ReadFile;
using eyelib::platform::filesystem::WriteFile;
using eyelib::platform::filesystem::FileExists;
using eyelib::platform::filesystem::IsDirectory;
using eyelib::platform::filesystem::GetFileSize;
using eyelib::platform::filesystem::GetFileTime;
using eyelib::platform::filesystem::CreateDirectory;
using eyelib::platform::filesystem::DeleteFile;
using eyelib::platform::filesystem::RenamePath;
using eyelib::platform::filesystem::DeleteDirectory;
using eyelib::platform::filesystem::ListDirectory;
using eyelib::platform::filesystem::GetAbsolutePath;
using eyelib::platform::filesystem::GetRelativePath;
using eyelib::platform::filesystem::NormalizePath;
using eyelib::platform::filesystem::GetExtension;
using eyelib::platform::filesystem::GetFilename;
using eyelib::platform::filesystem::GetDirectory;
using eyelib::platform::filesystem::GetExecutablePath;
using eyelib::platform::filesystem::GetEngineAssetsRoot;
using eyelib::platform::filesystem::SetWorkingDirectory;

} // namespace filesystem
} // namespace platform
} // namespace api
