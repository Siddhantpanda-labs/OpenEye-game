#include "FrameEncoder.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../../thirdparty/stb/stb_image_write.h"

#include <cstring>

namespace eyelib {
namespace app {

namespace {

void jpeg_write_to_vector(void* context, void* data, int size) {
    auto* vec = static_cast<std::vector<uint8_t>*>(context);
    const uint8_t* p = static_cast<const uint8_t*>(data);
    vec->insert(vec->end(), p, p + size);
}

} // namespace

bool EncodeRGBA8ToJPEG(const uint8_t* rgba, uint32_t width, uint32_t height, int quality,
                      std::vector<uint8_t>& outJpeg) {
    if (!rgba || width == 0 || height == 0) return false;
    outJpeg.clear();
    stbi_flip_vertically_on_write(0);  // We already have top-left origin
    const int w = static_cast<int>(width);
    const int h = static_cast<int>(height);
    const int result = stbi_write_jpg_to_func(
        jpeg_write_to_vector,
        &outJpeg,
        w, h, 4,
        rgba,
        (quality <= 0 || quality > 100) ? 75 : quality);
    return result != 0;
}

} // namespace app
} // namespace eyelib
