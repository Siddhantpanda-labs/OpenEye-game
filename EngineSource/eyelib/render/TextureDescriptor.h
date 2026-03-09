#pragma once

#include "eyelib/render/resources/TextureTypes.h"
#include <string>

namespace eyelib {
namespace render {

/**
 * CPU-only texture definition (Phase 3: asset vs GPU instance).
 * File path and load options; no RHI texture. Safe to store in registry and serialize.
 * Use with Texture2D::LoadFromDescriptor(device, desc) to create a GPU instance.
 */
struct TextureDescriptor {
    std::string filePath;
    bool isSRGB = false;
    bool generateMipmaps = true;
};

} // namespace render
} // namespace eyelib
