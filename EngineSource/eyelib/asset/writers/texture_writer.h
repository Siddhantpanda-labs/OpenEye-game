#pragma once

#include "asset/core/import_texture.h"

#include <string>

namespace asset {

/** Writes ImportTexture to canonical .texture binary, or copies source image. Stub: copy/extract path for now. */
class TextureWriter {
public:
    /** Write texture to path. If pixels empty, may just record source path in a minimal format. */
    static bool Write(const ImportTexture& texture, const std::string& path);
};

} // namespace asset
