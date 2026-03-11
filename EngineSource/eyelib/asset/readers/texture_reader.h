#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace asset {

/** Loaded texture from canonical .tex (pixel data only; used for GPU upload). */
struct LoadedTexture {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;  // 3 or 4
    std::vector<uint8_t> pixels;
};

/** Reads .tex file into LoadedTexture. Returns true if valid and pixels present. */
class TextureReader {
public:
    static bool Read(const std::string& path, LoadedTexture& out);
};

} // namespace asset
