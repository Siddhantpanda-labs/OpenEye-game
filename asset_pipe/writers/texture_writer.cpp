#include "asset_pipe/writers/texture_writer.h"
#include "asset_pipe/core/texture_format.h"

#include <fstream>
#include <cstdint>
#include <cstring>

namespace asset_pipe {

namespace {

using namespace TextureFormat;

} // namespace

bool TextureWriter::Write(const ImportTexture& texture, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    if (texture.pixels.empty()) {
        // Stub: write minimal header + source path for re-resolution later
        uint32_t magic = kMagic;
        uint32_t version = kVersion;
        uint32_t w = texture.width;
        uint32_t h = texture.height;
        uint32_t ch = texture.channels;
        uint32_t pathLen = static_cast<uint32_t>(texture.sourcePath.size());
        file.write(reinterpret_cast<const char*>(&magic), 4);
        file.write(reinterpret_cast<const char*>(&version), 4);
        file.write(reinterpret_cast<const char*>(&w), 4);
        file.write(reinterpret_cast<const char*>(&h), 4);
        file.write(reinterpret_cast<const char*>(&ch), 4);
        file.write(reinterpret_cast<const char*>(&pathLen), 4);
        if (pathLen) file.write(texture.sourcePath.data(), pathLen);
        return file.good();
    }

    // Full pixel data
    uint32_t magic = kMagic;
    uint32_t version = kVersion;
    uint32_t w = texture.width;
    uint32_t h = texture.height;
    uint32_t ch = texture.channels;
    uint32_t dataSize = static_cast<uint32_t>(texture.pixels.size());
    file.write(reinterpret_cast<const char*>(&magic), 4);
    file.write(reinterpret_cast<const char*>(&version), 4);
    file.write(reinterpret_cast<const char*>(&w), 4);
    file.write(reinterpret_cast<const char*>(&h), 4);
    file.write(reinterpret_cast<const char*>(&ch), 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
    file.write(reinterpret_cast<const char*>(texture.pixels.data()), texture.pixels.size());
    return file.good();
}

} // namespace asset_pipe
