#include "eyelib/render/PipelineCache.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <algorithm>
#include <sstream>

namespace eyelib {
namespace render {

namespace {
std::string s_cacheDir = "shader_cache";
} // namespace

void SetPipelineCacheDirectory(const std::string& dir) {
    s_cacheDir = dir;
}

std::string GetPipelineCacheDirectory() {
    return s_cacheDir;
}

std::string PipelineCacheKey(const std::string& shaderName,
                              rhi::PrimitiveTopology topology,
                              const std::vector<std::string>& keywords) {
    std::ostringstream o;
    o << shaderName << "_" << static_cast<int>(topology);
    std::vector<std::string> sorted = keywords;
    std::sort(sorted.begin(), sorted.end());
    for (const auto& k : sorted) {
        if (k.empty()) continue;
        o << "_" << k;
    }
    std::string s = o.str();
    for (char& c : s) {
        if (c == '/' || c == '\\') c = '_';
    }
    return s;
}

bool SavePipelineBinary(const std::string& key, uint32_t format, const void* data, size_t size) {
    if (s_cacheDir.empty() || !data) return false;
    std::string path = s_cacheDir + "/" + key + ".bin";
    if (!platform::filesystem::CreateDirectory(s_cacheDir))
        return false;
    std::vector<uint8_t> blob;
    blob.resize(8 + size);
    uint32_t fmt = format;
    uint32_t sz = static_cast<uint32_t>(size);
    memcpy(blob.data(), &fmt, 4);
    memcpy(blob.data() + 4, &sz, 4);
    memcpy(blob.data() + 8, data, size);
    return platform::filesystem::WriteFile(path, blob.data(), blob.size());
}

bool LoadPipelineBinary(const std::string& key, uint32_t& outFormat, std::vector<uint8_t>& outData) {
    if (s_cacheDir.empty()) return false;
    std::string path = s_cacheDir + "/" + key + ".bin";
    outData = platform::filesystem::ReadFile(path);
    if (outData.size() < 8) return false;
    memcpy(&outFormat, outData.data(), 4);
    uint32_t size = 0;
    memcpy(&size, outData.data() + 4, 4);
    if (outData.size() != 8 + size) return false;
    outData.erase(outData.begin(), outData.begin() + 8);
    return true;
}

} // namespace render
} // namespace eyelib
