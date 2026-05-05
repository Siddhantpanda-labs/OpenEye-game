#pragma once

#include "eyelib/render/resources/TextureTypes.h"
#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace render {

/**
 * @brief Texture compression utilities (placeholder).
 * BC (DXT), ASTC, ETC2 can be added when needed.
 */
enum class CompressionFormat {
    None,
    BC1,
    BC3,
    BC5,
    BC7,
    ASTC,
    ETC2
};

/** Check if a compression format is supported on this device. */
inline bool IsCompressionSupported(CompressionFormat fmt) {
    (void)fmt;
    return false;
}

} // namespace render
} // namespace eyelib
