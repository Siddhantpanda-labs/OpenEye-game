#pragma once

#include "asset/core/math_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace asset {

struct VectorKey {
    float time = 0.f;
    Vec3 value{};
};

struct QuatKey {
    float time = 0.f;
    Vec4 value{};  // x, y, z, w
};

struct AnimationChannel {
    std::string boneName;
    int boneIndex = -1;

    std::vector<VectorKey> positionKeys;
    std::vector<QuatKey> rotationKeys;
    std::vector<VectorKey> scaleKeys;
};

struct ImportAnimation {
    std::string name;
    float duration = 0.f;
    std::vector<AnimationChannel> channels;
};

} // namespace asset
