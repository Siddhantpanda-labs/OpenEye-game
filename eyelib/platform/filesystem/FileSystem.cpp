#include "platform/filesystem/FileSystem.h"
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// Avoid Windows macro collisions with our filesystem API.
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef DeleteFile
#undef DeleteFile
#endif
#else
#include <sys/stat.h>
#endif
#if defined(__linux__) || defined(__ANDROID__)
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace eyelib {
namespace platform {
namespace filesystem {

namespace {
int64_t GetFileTimeEpochSeconds(const std::string& path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fa = {};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fa))
        return 0;
    ULARGE_INTEGER u;
    u.LowPart = fa.ftLastWriteTime.dwLowDateTime;
    u.HighPart = fa.ftLastWriteTime.dwHighDateTime;
    const int64_t WINDOWS_TICKS = 10000000;
    const int64_t UNIX_EPOCH = 11644473600LL; // seconds between 1601 and 1970
    return static_cast<int64_t>(u.QuadPart / WINDOWS_TICKS - UNIX_EPOCH);
#else
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<int64_t>(st.st_mtime);
#endif
}
} // namespace

std::vector<uint8_t> ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = f.tellg();
    if (size <= 0) return {};
    f.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(buf.data()), size)) return {};
    return buf;
}

bool WriteFile(const std::string& path, const void* data, size_t size) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    return static_cast<bool>(f.write(static_cast<const char*>(data), size));
}

bool FileExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec) && !ec;
}

size_t GetFileSize(const std::string& path) {
    std::error_code ec;
    auto sz = fs::file_size(path, ec);
    if (ec || !fs::is_regular_file(path, ec)) return 0;
    return static_cast<size_t>(sz);
}

int64_t GetFileTime(const std::string& path) {
    return GetFileTimeEpochSeconds(path);
}

bool CreateDirectory(const std::string& path) {
    std::error_code ec;
    return fs::create_directories(path, ec) || (fs::exists(path, ec) && fs::is_directory(path, ec));
}

bool DeleteFile(const std::string& path) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || ec) return false;
    return fs::remove(path, ec) && !ec;
}

std::vector<std::string> ListDirectory(const std::string& path) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(path, ec)) {
        if (ec) return {};
        out.push_back(e.path().filename().string());
    }
    return out;
}

std::string GetAbsolutePath(const std::string& path) {
    std::error_code ec;
    return fs::absolute(path, ec).string();
}

std::string GetRelativePath(const std::string& absolute, const std::string& base) {
    std::error_code ec;
    auto r = fs::relative(absolute, base, ec);
    return ec ? std::string{} : r.string();
}

std::string NormalizePath(const std::string& path) {
    return fs::path(path).lexically_normal().make_preferred().string();
}

std::string GetExtension(const std::string& path) {
    return fs::path(path).extension().string();
}

std::string GetFilename(const std::string& path) {
    return fs::path(path).filename().string();
}

std::string GetDirectory(const std::string& path) {
    return fs::path(path).parent_path().string();
}

std::string GetExecutablePath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, static_cast<DWORD>(sizeof(buf)));
    if (n == 0 || n >= sizeof(buf))
        return {};
    return NormalizePath(std::string(buf, n));
#elif defined(__linux__) || defined(__ANDROID__)
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return {};
    buf[n] = '\0';
    return NormalizePath(buf);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = static_cast<uint32_t>(sizeof(buf));
    if (_NSGetExecutablePath(buf, &size) != 0)
        return {};
    return NormalizePath(buf);
#else
    return {};
#endif
}

} // namespace filesystem
} // namespace platform
} // namespace eyelib
