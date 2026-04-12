#pragma once

#include "eyelib/rendering/resources/Texture2D.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace rendering {

/** Decoded image (CPU-side). Create Texture2D on main thread via CreateFromDecoded. */
struct DecodedImage {
    int width = 0;
    int height = 0;
    int channels = 0;
    bool isHDR = false;
    std::vector<uint8_t> ldr;
    std::vector<float> hdr;
};

/**
 * @brief Texture loading helpers. LoadPNG/LoadJPEG/LoadHDR, LoadAsync (decode-only).
 */
class TextureLoader {
public:
    using Texture2DPtr = std::shared_ptr<Texture2D>;
    using LoadCallback = std::function<void(Texture2DPtr)>;
    using DecodeCallback = std::function<void(DecodedImage)>;

    static Texture2DPtr LoadPNG(const std::string& path, bool isSRGB = false, bool generateMipmaps = true);
    /** Load with explicit device (recommended). */
    static Texture2DPtr LoadPNG(rhi::IDevice* device, const std::string& path, bool isSRGB = false, bool generateMipmaps = true);
    static Texture2DPtr LoadJPEG(const std::string& path, bool isSRGB = false, bool generateMipmaps = true);
    static Texture2DPtr LoadJPEG(rhi::IDevice* device, const std::string& path, bool isSRGB = false, bool generateMipmaps = true);
    static Texture2DPtr LoadHDR(const std::string& path, bool generateMipmaps = false);
    static Texture2DPtr LoadHDR(rhi::IDevice* device, const std::string& path, bool generateMipmaps = false);

    /** Async decode only. Callback runs on worker; create Texture2D from DecodedImage on main thread. */
    static void LoadAsync(const std::string& path, DecodeCallback callback);

    /** Create Texture2D from DecodedImage. Call on main thread (uses GL). */
    static Texture2DPtr CreateFromDecoded(const DecodedImage& img, bool isSRGB = false, bool generateMipmaps = true);
    /** Create with explicit device (recommended). */
    static Texture2DPtr CreateFromDecoded(rhi::IDevice* device, const DecodedImage& img, bool isSRGB = false, bool generateMipmaps = true);
};

} // namespace rendering
} // namespace eyelib
