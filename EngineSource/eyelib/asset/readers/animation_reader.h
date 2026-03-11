#pragma once

#include "asset/core/import_animation.h"

#include <string>

namespace asset {

/** Reads canonical .anim binary into ImportAnimation. Returns true on success. */
class AnimationReader {
public:
    static bool Read(const std::string& path, ImportAnimation& out);
};

} // namespace asset
