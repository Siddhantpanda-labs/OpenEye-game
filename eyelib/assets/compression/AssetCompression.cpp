#include "AssetCompression.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/core/Log.h"

#if defined(EYELIB_WITH_LZ4) && EYELIB_WITH_LZ4
#include <lz4.h>
#endif
#if defined(EYELIB_WITH_ZSTD) && EYELIB_WITH_ZSTD
#include <zstd.h>
#endif

#include <cstring>
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

    if (format == CompressionFormat::LZ4) {
#if defined(EYELIB_WITH_LZ4) && EYELIB_WITH_LZ4
        if (input.empty()) { outData.clear(); return true; }
        const int srcSize = static_cast<int>(input.size());
        const int bound = LZ4_compressBound(srcSize);
        if (bound <= 0) return false;
        std::vector<uint8_t> compressed(static_cast<size_t>(bound));
        const int n = LZ4_compress_default(
            reinterpret_cast<const char*>(input.data()),
            reinterpret_cast<char*>(compressed.data()), srcSize, bound);
        if (n <= 0) return false;
        compressed.resize(static_cast<size_t>(n));
        outData.clear();
        outData.reserve(8 + compressed.size());
        uint64_t rawSize = static_cast<uint64_t>(input.size());
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&rawSize);
        outData.insert(outData.end(), p, p + 8);
        outData.insert(outData.end(), compressed.begin(), compressed.end());
        return true;
#else
        core::Log::Warn("AssetCompression: LZ4 not enabled, using RLE fallback.");
        return RLECompress(input, outData);
#endif
    }

    if (format == CompressionFormat::Zstd) {
#if defined(EYELIB_WITH_ZSTD) && EYELIB_WITH_ZSTD
        if (input.empty()) { outData.clear(); return true; }
        const size_t srcSize = input.size();
        const size_t bound = ZSTD_compressBound(srcSize);
        if (ZSTD_isError(bound)) return false;
        std::vector<uint8_t> compressed(bound);
        const size_t n = ZSTD_compress(compressed.data(), bound, input.data(), srcSize, 3);
        if (ZSTD_isError(n)) return false;
        compressed.resize(n);
        outData = std::move(compressed);
        return true;
#else
        core::Log::Warn("AssetCompression: Zstd not enabled, using RLE fallback.");
        return RLECompress(input, outData);
#endif
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
    if (format == CompressionFormat::LZ4) {
#if defined(EYELIB_WITH_LZ4) && EYELIB_WITH_LZ4
        if (data.size() < 8) return false;
        uint64_t rawSize = 0;
        std::memcpy(&rawSize, data.data(), 8);
        const size_t payloadSize = data.size() - 8;
        outData.resize(static_cast<size_t>(rawSize));
        const int n = LZ4_decompress_safe(
            reinterpret_cast<const char*>(data.data() + 8),
            reinterpret_cast<char*>(outData.data()),
            static_cast<int>(payloadSize),
            static_cast<int>(rawSize));
        if (n < 0) return false;
        outData.resize(static_cast<size_t>(n));
        return true;
#else
        core::Log::Warn("AssetCompression: LZ4 not enabled, using RLE fallback.");
        return RLEDecompress(data, outData);
#endif
    }
    if (format == CompressionFormat::Zstd) {
#if defined(EYELIB_WITH_ZSTD) && EYELIB_WITH_ZSTD
        const unsigned long long size = ZSTD_getFrameContentSize(data.data(), data.size());
        if (size == ZSTD_CONTENTSIZE_ERROR || size == ZSTD_CONTENTSIZE_UNKNOWN) return false;
        outData.resize(static_cast<size_t>(size));
        const size_t n = ZSTD_decompress(outData.data(), size, data.data(), data.size());
        if (ZSTD_isError(n)) return false;
        outData.resize(n);
        return true;
#else
        core::Log::Warn("AssetCompression: Zstd not enabled, using RLE fallback.");
        return RLEDecompress(data, outData);
#endif
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
