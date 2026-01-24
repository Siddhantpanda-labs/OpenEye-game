#include "AssetCompression.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/core/Log.h"

#include <fstream>
#include <iterator>

namespace eyelib {
namespace assets {

namespace {
bool ReadFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

bool RLECompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    output.clear();
    if (input.empty()) return true;
    size_t i = 0;
    while (i < input.size()) {
        uint8_t value = input[i];
        size_t run = 1;
        while (i + run < input.size() && input[i + run] == value && run < 255) {
            ++run;
        }
        output.push_back(static_cast<uint8_t>(run));
        output.push_back(value);
        i += run;
    }
    return true;
}

bool RLEDecompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    output.clear();
    if (input.size() % 2 != 0) return false;
    for (size_t i = 0; i < input.size(); i += 2) {
        uint8_t count = input[i];
        uint8_t value = input[i + 1];
        output.insert(output.end(), count, value);
    }
    return true;
}
} // namespace

bool AssetCompression::CompressAsset(const AssetId& id, CompressionFormat format, std::vector<uint8_t>& outData) {
    auto& registry = AssetManager::Get().Registry();
    const std::string path = registry.GetFilePath(id);
    if (path.empty()) return false;
    std::vector<uint8_t> input;
    if (!ReadFile(path, input)) return false;

    if (format == CompressionFormat::None) {
        outData = std::move(input);
        return true;
    }

    if (format == CompressionFormat::LZ4 || format == CompressionFormat::Zstd) {
        core::Log::Warn("AssetCompression: LZ4/Zstd not enabled, using RLE fallback.");
        return RLECompress(input, outData);
    }
    return false;
}

bool AssetCompression::DecompressAsset(const AssetId& id, CompressionFormat format,
                                       const std::vector<uint8_t>& data, std::vector<uint8_t>& outData) {
    (void)id;
    if (format == CompressionFormat::None) {
        outData = data;
        return true;
    }
    if (format == CompressionFormat::LZ4 || format == CompressionFormat::Zstd) {
        core::Log::Warn("AssetCompression: LZ4/Zstd not enabled, using RLE fallback.");
        return RLEDecompress(data, outData);
    }
    return false;
}

bool AssetCompression::CompressTexture(const std::vector<uint8_t>& rgba, int width, int height,
                                       TextureCompressionFormat format, std::vector<uint8_t>& outData) {
    (void)rgba;
    (void)width;
    (void)height;
    (void)format;
    core::Log::Warn("AssetCompression: texture compression not available in this build.");
    outData.clear();
    return false;
}

} // namespace assets
} // namespace eyelib
