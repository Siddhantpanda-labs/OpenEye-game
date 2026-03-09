#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace platform {
namespace filesystem {

/** Read entire file as binary. Returns empty vector on failure. */
std::vector<uint8_t> ReadFile(const std::string& path);

/** Write binary data to file. Returns false on failure. */
bool WriteFile(const std::string& path, const void* data, size_t size);

/** Overload for vector. */
inline bool WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    return WriteFile(path, data.data(), data.size());
}

/** True if path exists (file or directory). */
bool FileExists(const std::string& path);

/** File size in bytes. Returns 0 if not a file or error. */
size_t GetFileSize(const std::string& path);

/** Last modification time as seconds since Unix epoch. Returns 0 on failure. */
int64_t GetFileTime(const std::string& path);

/** Create directory (and parents). Returns false on failure. */
bool CreateDirectory(const std::string& path);

/** Delete file. Returns false on failure. */
bool DeleteFile(const std::string& path);

/** List direct contents (files and dirs). Returns empty on error. */
std::vector<std::string> ListDirectory(const std::string& path);

/** Resolve to absolute path. */
std::string GetAbsolutePath(const std::string& path);

/** Get path relative to base. */
std::string GetRelativePath(const std::string& absolute, const std::string& base);

/** Normalize slashes and resolve . / .. */
std::string NormalizePath(const std::string& path);

/** Extension including dot, e.g. ".txt". Empty if none. */
std::string GetExtension(const std::string& path);

/** Filename only, e.g. "foo.txt". */
std::string GetFilename(const std::string& path);

/** Parent directory path. */
std::string GetDirectory(const std::string& path);

/** Full path to the current executable. Empty on failure. */
std::string GetExecutablePath();

} // namespace filesystem
} // namespace platform
} // namespace eyelib
