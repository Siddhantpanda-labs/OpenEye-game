#include "asset/readers/texture_reader.h"
#include "asset/core/texture_format.h"

#include <algorithm>
#include <fstream>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace asset {

static bool IsImagePath(const std::string& path) {
    if (path.size() < 4) return false;
    std::string ext = path.substr(path.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == ".png" || ext == ".jpg") return true;
    if (path.size() >= 5) {
        std::string ext5 = path.substr(path.size() - 5);
        std::transform(ext5.begin(), ext5.end(), ext5.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext5 == ".jpeg") return true;
    }
    return false;
}

bool TextureReader::Read(const std::string& path, LoadedTexture& out) {
    if (IsImagePath(path)) {
        out = LoadedTexture{};
        int w = 0, h = 0, ch = 0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
        if (!data || w <= 0 || h <= 0) return false;
        out.width = static_cast<uint32_t>(w);
        out.height = static_cast<uint32_t>(h);
        out.channels = 4;
        out.pixels.assign(data, data + static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
        stbi_image_free(data);
        return true;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    using namespace TextureFormat;

    uint32_t magic = 0, version = 0, w = 0, h = 0, ch = 0;
    if (!file.read(reinterpret_cast<char*>(&magic), 4)) return false;
    if (!file.read(reinterpret_cast<char*>(&version), 4)) return false;
    if (!file.read(reinterpret_cast<char*>(&w), 4)) return false;
    if (!file.read(reinterpret_cast<char*>(&h), 4)) return false;
    if (!file.read(reinterpret_cast<char*>(&ch), 4)) return false;
    if (magic != kMagic || (version != kVersion && version != kVersionV1) || w == 0 || h == 0) return false;

    uint32_t storage = kStorageRawPixels;
    uint32_t dataSize = 0;
    if (version == kVersion) {
        if (!file.read(reinterpret_cast<char*>(&storage), 4)) return false;
    }
    if (!file.read(reinterpret_cast<char*>(&dataSize), 4)) return false;
    if (dataSize == 0) return false;

    out = LoadedTexture{};
    if (storage == kStorageEncodedImage) {
        std::vector<uint8_t> encoded(dataSize);
        if (!file.read(reinterpret_cast<char*>(encoded.data()), dataSize)) return false;
        int dw = 0, dh = 0, dch = 0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char* decoded = stbi_load_from_memory(
            reinterpret_cast<const unsigned char*>(encoded.data()),
            static_cast<int>(encoded.size()),
            &dw, &dh, &dch, 4);
        if (!decoded) return false;
        const size_t total = static_cast<size_t>(dw) * static_cast<size_t>(dh) * 4u;
        out.width = static_cast<uint32_t>(dw);
        out.height = static_cast<uint32_t>(dh);
        out.channels = 4;
        out.pixels.assign(decoded, decoded + total);
        stbi_image_free(decoded);
        return true;
    }

    if (ch != 1 && ch != 3 && ch != 4) return false;
    if (dataSize != w * h * ch) return false;
    out.width = w;
    out.height = h;
    out.channels = ch;
    out.pixels.resize(dataSize);
    if (!file.read(reinterpret_cast<char*>(out.pixels.data()), dataSize)) return false;
    return true;
}

} // namespace asset
