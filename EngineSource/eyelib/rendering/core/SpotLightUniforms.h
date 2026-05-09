#pragma once

#include "eyelib/math/Types.h"

#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace rendering {

inline constexpr uint32_t kMaxSpotLights = 16u;
inline constexpr uint32_t kSpotLightUniformGpuBytes = 16u + (kMaxSpotLights * 128u); // count + array of lights

struct SpotLightStd140 {
    math::Vec4 positionAndRange;      // xyz = position, w = range
    math::Vec4 directionAndInnerCone; // xyz = direction, w = innerConeCos
    math::Vec4 colorAndOuterCone;     // xyz = color * intensity, w = outerConeCos
    math::Mat4 lightViewProj;
    math::Vec4 shadowParams;          // x = shadowMapIndex (-1 if none), yzw = padding
};
static_assert(sizeof(SpotLightStd140) == 128u, "SpotLightStd140 must be 128 bytes");

struct SpotLightArrayStd140 {
    math::Vec4 countAndPadding; // x = count (as float/int)
    SpotLightStd140 lights[kMaxSpotLights];
};
static_assert(sizeof(SpotLightArrayStd140) == kSpotLightUniformGpuBytes, "SpotLightArrayStd140 size mismatch");

}  // namespace rendering
}  // namespace eyelib
