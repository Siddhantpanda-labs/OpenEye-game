#pragma once

#include "asset/core/guid.h"

#include <vector>

namespace asset {

class AssetCache;

enum class CompressionFormat {
    None = 0,
    LZ4,
    Zstd
};

enum class TextureCompressionFormat {
    None = 0,
    BC7,
    ASTC,
    ETC2
};

/**
 * Compress/decompress asset bytes (e.g. for packages or streaming).
 * Asset data resolved via cache; implementations can use LZ4/Zstd when available.
 */
class Compression {
public:
    /** Compress asset bytes (from cache by guid) into outData. */
    static bool CompressAsset(const Guid& id, AssetCache* cache, CompressionFormat format, std::vector<uint8_t>& outData);
    /** Decompress data into outData. */
    static bool DecompressAsset(const Guid& id, AssetCache* cache, CompressionFormat format,
                               const std::vector<uint8_t>& data, std::vector<uint8_t>& outData);

    /** Compress raw RGBA texture. */
    static bool CompressTexture(const std::vector<uint8_t>& rgba, int width, int height,
                               TextureCompressionFormat format, std::vector<uint8_t>& outData);
};

} // namespace asset
