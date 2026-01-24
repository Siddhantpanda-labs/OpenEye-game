#include "eyelib/render/resources/TextureLoader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/core/Log.h"
#include "eyelib/core/job/ThreadPool.h"
#include "eyelib/platform/filesystem/FileSystem.h"

#include <stb_image.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

namespace eyelib {
namespace render {

namespace {
static bool IsHDRPath(const std::string& path) {
    size_t n = path.size();
    if (n < 4) return false;
    std::string ext = path.substr(n - 4);
    for (char& c : ext) if (c >= 'A' && c <= 'Z') c += 32;
    return ext == ".hdr";
}
} // namespace

TextureLoader::Texture2DPtr TextureLoader::LoadPNG(const std::string& path, bool isSRGB, bool generateMipmaps) {
    auto tex = std::make_shared<Texture2D>();
    if (tex->LoadFromFile(path, isSRGB, generateMipmaps)) return tex;
    return nullptr;
}

TextureLoader::Texture2DPtr TextureLoader::LoadJPEG(const std::string& path, bool isSRGB, bool generateMipmaps) {
    return LoadPNG(path, isSRGB, generateMipmaps);
}

TextureLoader::Texture2DPtr TextureLoader::LoadHDR(const std::string& path, bool generateMipmaps) {
    std::vector<uint8_t> bytes = eyelib::platform::filesystem::ReadFile(path);
    if (bytes.empty()) {
        core::Log::ErrorF("TextureLoader::LoadHDR: failed to read %s", path.c_str());
        return nullptr;
    }
    stbi_set_flip_vertically_on_load(1);
    int w = 0, h = 0, ch = 0;
    float* data = stbi_loadf_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &ch, 0);
    if (!data) {
        core::Log::ErrorF("TextureLoader::LoadHDR: stbi_loadf failed: %s", stbi_failure_reason());
        return nullptr;
    }
    auto tex = std::make_shared<Texture2D>();
    TextureFormat fmt = (ch == 3) ? TextureFormat::RGB16F : TextureFormat::RGBA16F;
    if (!tex->Create(w, h, fmt, data)) {
        stbi_image_free(data);
        return nullptr;
    }
    stbi_image_free(data);
    if (generateMipmaps) tex->GenerateMipmaps();
    return tex;
}

void TextureLoader::LoadAsync(const std::string& path, DecodeCallback callback) {
    if (!callback) return;
    static core::job::ThreadPool s_pool(0);
    s_pool.Submit([path, callback]() {
        DecodedImage img;
        std::vector<uint8_t> bytes = eyelib::platform::filesystem::ReadFile(path);
        if (bytes.empty()) {
            callback(std::move(img));
            return;
        }
        if (IsHDRPath(path)) {
            stbi_set_flip_vertically_on_load(1);
            int w = 0, h = 0, ch = 0;
            float* data = stbi_loadf_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &ch, 0);
            if (!data) {
                callback(std::move(img));
                return;
            }
            img.width = w;
            img.height = h;
            img.channels = ch;
            img.isHDR = true;
            size_t n = static_cast<size_t>(w * h * ch);
            img.hdr.resize(n);
            std::memcpy(img.hdr.data(), data, n * sizeof(float));
            stbi_image_free(data);
        } else {
            stbi_set_flip_vertically_on_load(0);
            int w = 0, h = 0, ch = 0;
            unsigned char* data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &ch, 0);
            if (!data) {
                callback(std::move(img));
                return;
            }
            img.width = w;
            img.height = h;
            img.channels = ch;
            img.isHDR = false;
            size_t n = static_cast<size_t>(w * h * ch);
            img.ldr.resize(n);
            std::memcpy(img.ldr.data(), data, n);
            stbi_image_free(data);
        }
        callback(std::move(img));
    });
}

TextureLoader::Texture2DPtr TextureLoader::CreateFromDecoded(const DecodedImage& img, bool isSRGB, bool generateMipmaps) {
    if (img.width <= 0 || img.height <= 0) return nullptr;
    auto tex = std::make_shared<Texture2D>();
    if (img.isHDR) {
        TextureFormat fmt = (img.channels == 3) ? TextureFormat::RGB16F : TextureFormat::RGBA16F;
        if (!tex->Create(img.width, img.height, fmt, img.hdr.data())) return nullptr;
    } else {
        if (img.ldr.empty()) return nullptr;
        TextureFormat fmt = TextureFormat::RGBA8;
        if (img.channels == 1) fmt = TextureFormat::R8;
        else if (img.channels == 2) fmt = TextureFormat::RG8;
        else if (img.channels == 3) fmt = TextureFormat::RGB8;
        if (!tex->Create(img.width, img.height, fmt, img.ldr.data())) return nullptr;
    }
    if (generateMipmaps) tex->GenerateMipmaps();
    return tex;
}

} // namespace render
} // namespace eyelib
