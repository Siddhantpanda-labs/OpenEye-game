#pragma once

#include <string>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Public CPU-only texture definition consumed by `ResourceCache::CreateTexture`.
 *
 * Mirrors the internal `eyelib::rendering::TextureDescriptor`. The cache loads the
 * file via the new module's `eyelib::rendering::Texture2D::LoadFromDescriptor` and
 * returns an opaque `Texture2D` handle. The cache also assigns each created texture
 * a stable `uint32_t` token (the "texture handle"); the token is consumed by
 * `MaterialRecord::*Texture` slots and resolved at draw-time when the new module's
 * texture binding-table feature lands. Tokens stay non-zero and stable for the
 * texture's lifetime (so `MaterialRecord` rows authored today are forward-compatible).
 *
 * Safe to copy / move / store. POD-ish.
 */
struct TextureData {
    /** Absolute or asset-relative file path. PNG / JPG / TGA / etc. supported via stb_image. */
    std::string filePath;

    /** True for color textures (sRGB → linear conversion at sample time). */
    bool isSRGB = false;

    /** Generate a full mip chain at upload time. */
    bool generateMipmaps = true;
};

}  // namespace rendering
}  // namespace api
}  // namespace eyelib
