#pragma once

#include "asset/core/import_animation.h"

#include <string>

namespace asset {

/** Writes ImportAnimation to canonical .anim binary. Stub for phase 1. */
class AnimationWriter {
public:
    static bool Write(const ImportAnimation& animation, const std::string& path);
};

} // namespace asset
