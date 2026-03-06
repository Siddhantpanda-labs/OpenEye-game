#pragma once

#include <cstdint>
#include <vector>

namespace eyelib {
namespace app {

/** Encode RGBA8 (row-major, top-left) to JPEG. quality 1-100. Returns true on success. */
bool EncodeRGBA8ToJPEG(const uint8_t* rgba, uint32_t width, uint32_t height, int quality,
                      std::vector<uint8_t>& outJpeg);

} // namespace app
} // namespace eyelib
