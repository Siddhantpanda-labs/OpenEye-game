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

    // Full payload data (raw pixels or encoded image bytes)
    uint32_t magic = kMagic;
    uint32_t version = kVersion;
    uint32_t w = texture.width;
    uint32_t h = texture.height;
    uint32_t ch = texture.channels ? texture.channels : 4u;
    uint32_t storage = kStorageRawPixels;
    const std::vector<uint8_t>* payload = &texture.pixels;
    if (!texture.encodedData.empty()) {
        storage = kStorageEncodedImage;
        payload = &texture.encodedData;
    }
    if (payload->empty()) return false;
    uint32_t dataSize = static_cast<uint32_t>(payload->size());

    file.write(reinterpret_cast<const char*>(&magic), 4);
    file.write(reinterpret_cast<const char*>(&version), 4);
    file.write(reinterpret_cast<const char*>(&w), 4);
    file.write(reinterpret_cast<const char*>(&h), 4);
    file.write(reinterpret_cast<const char*>(&ch), 4);
    file.write(reinterpret_cast<const char*>(&storage), 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
    file.write(reinterpret_cast<const char*>(payload->data()), payload->size());
    return file.good();
}

} // namespace asset_pipe
