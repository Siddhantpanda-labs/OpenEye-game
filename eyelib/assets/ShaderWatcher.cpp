#include "eyelib/assets/ShaderWatcher.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <chrono>
#include <filesystem>
#include <set>
#include <string>

namespace eyelib {
namespace assets {

namespace {

const std::set<std::string>& ShaderExtensions() {
    static const std::set<std::string> s = { ".vert", ".frag", ".glsl", ".comp", ".geom", ".tesc", ".tese" };
    return s;
}

bool IsShaderFile(const std::string& path) {
    std::string ext = platform::filesystem::GetExtension(path);
    if (ext.empty()) return false;
    for (char& c : ext) if (c >= 'A' && c <= 'Z') c += 32;
    return ShaderExtensions().count(ext) != 0;
}

void CollectShaderFiles(const std::string& dir, std::vector<std::pair<std::string, std::string>>& out) {
    namespace fs = std::filesystem;
    const fs::path base(dir);
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) continue;
        if (!it->is_regular_file(ec)) continue;
        const fs::path p = it->path();
        std::string rel = fs::relative(p, base, ec).string();
        if (ec || rel.empty()) continue;
#ifdef _WIN32
        for (char& c : rel) if (c == '\\') c = '/';
#endif
        if (!IsShaderFile(rel)) continue;
        out.emplace_back(p.string(), rel);
    }
}

} // namespace

int ShaderWatcher::Tick() {
    if (m_sourceAssetsDir.empty() || m_runtimeAssetsDir.empty()) return 0;

    const int64_t nowMs = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    if (m_lastPollTime != 0 && (nowMs - m_lastPollTime) < kPollIntervalMs)
        return 0;
    m_lastPollTime = nowMs;

    const std::string sourceShaders = platform::filesystem::NormalizePath(m_sourceAssetsDir + "/shaders");
    const std::string runtimeShaders = platform::filesystem::NormalizePath(m_runtimeAssetsDir + "/shaders");

    if (!platform::filesystem::FileExists(sourceShaders)) return 0;

    std::vector<std::pair<std::string, std::string>> files;
    CollectShaderFiles(sourceShaders, files);

    int copied = 0;
    for (const auto& [absSource, relPath] : files) {
        const std::string targetPath = platform::filesystem::NormalizePath(runtimeShaders + "/" + relPath);
        const int64_t srcTime = platform::filesystem::GetFileTime(absSource);
        if (srcTime <= 0) continue;

        const int64_t dstTime = platform::filesystem::FileExists(targetPath)
            ? platform::filesystem::GetFileTime(targetPath) : 0;
        if (srcTime <= dstTime) continue;

        std::vector<uint8_t> data = platform::filesystem::ReadFile(absSource);
        if (data.empty()) continue;

        const std::string targetDir = platform::filesystem::GetDirectory(targetPath);
        if (!platform::filesystem::CreateDirectory(targetDir)) continue;

        if (!platform::filesystem::WriteFile(targetPath, data.data(), data.size())) {
            core::Log::WarnF("ShaderWatcher: failed to write %s", targetPath.c_str());
            continue;
        }
        copied++;
        if (m_onCopied) m_onCopied(std::string("shaders/") + relPath);
    }
    return copied;
}

} // namespace assets
} // namespace eyelib
