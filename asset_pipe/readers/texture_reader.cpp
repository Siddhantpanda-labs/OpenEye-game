#include "asset_pipe/readers/texture_reader.h"
#include "asset_pipe/core/texture_format.h"

#include <fstream>

namespace asset_pipe {

bool TextureReader::Read(const std::string& path, LoadedTexture& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    using namespace TextureFormat;

    uint32_t magic = 0, version = 0, w = 0, h = 0, ch = 0;
    if (!file.read(reinterpret_cast<char*>(&magic), 4)) return false;
    if (!file.read(reinterpret_cast<char*>(&version), 4)) return false;
    if (!file.read(reinterpret_cast<char*>(&w), 4)) return false;
    if (!file.read(reinterpret_cast<char*>(&h), 4)) return false;
    if (!file.read(reinterpret_cast<char*>(&ch), 4)) return false;
    if (magic != kMagic || version != kVersion || w == 0 || h == 0) return false;
    if (ch != 1 && ch != 3 && ch != 4) return false;

    uint32_t dataSize = 0;
    if (!file.read(reinterpret_cast<char*>(&dataSize), 4)) return false;
    if (dataSize == 0) return false;  // stub format has no pixels
    if (dataSize != w * h * ch) return false;

    out = LoadedTexture{};
    out.width = w;
    out.height = h;
    out.channels = ch;
    out.pixels.resize(dataSize);
    if (!file.read(reinterpret_cast<char*>(out.pixels.data()), dataSize)) return false;
    return true;
}

} // namespace asset_pipe
