#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace asset_pipe {

struct ImportTexture {
    std::string name;
    std::string sourcePath;  // path to source image (e.g. .png)

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;  // 1, 3, or 4
    std::vector<uint8_t> pixels;
    // Optional compressed/original image payload (PNG/JPG/etc.).
    // When present, writers should preserve this instead of expanded RGBA pixels.
    std::vector<uint8_t> encodedData;
};

} // namespace asset_pipe
