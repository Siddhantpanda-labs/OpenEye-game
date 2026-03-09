#pragma once

#include "eyelib/assets/AssetId.h"

#include <string>
#include <vector>

namespace eyelib {
namespace assets {

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

class AssetCompression {
public:
    static bool CompressAsset(const AssetId& id, CompressionFormat format, std::vector<uint8_t>& outData);
    static bool DecompressAsset(const AssetId& id, CompressionFormat format, const std::vector<uint8_t>& data,
                                std::vector<uint8_t>& outData);

    static bool CompressTexture(const std::vector<uint8_t>& rgba, int width, int height,
                                TextureCompressionFormat format, std::vector<uint8_t>& outData);
};

} // namespace assets
} // namespace eyelib
