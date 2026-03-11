#pragma once

#include "asset_pipe/core/import_animation.h"

#include <string>

namespace asset_pipe {

/** Reads canonical .anim binary into ImportAnimation. Returns true on success. */
class AnimationReader {
public:
    static bool Read(const std::string& path, ImportAnimation& out);
};

} // namespace asset_pipe
