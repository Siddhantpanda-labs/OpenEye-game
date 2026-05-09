#pragma once

#include "eyelib/math/Types.h"

#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace rendering {

/** Max omnidirectional lights in the deferred UBO (cost-controlled). */
inline constexpr uint32_t kMaxPointLights = 8u;
/** Max point lights that may allocate cubemap shadow faces (6 layers each). */
inline constexpr uint32_t kMaxPointShadowCasters = 4u;

inline constexpr uint32_t kPointLightFaces = 6u;

struct PointLightStd140 {
    math::Vec4 positionAndRange{};
    math::Vec4 colorIntensity{};  // xyz = color * intensity, w = linear intensity (for tuning)
    math::Vec4 shadowParams{};    // x = shadow layer base (0, 6, 12, …) or -1 if unshadowed
    math::Mat4 faceViewProj[kPointLightFaces]{};
};
static_assert(sizeof(PointLightStd140) == 432u, "PointLightStd140 std140 size");

struct PointLightArrayStd140 {
    math::Vec4 countAndPadding{};
    PointLightStd140 lights[kMaxPointLights]{};
};

inline constexpr uint32_t kPointLightUniformGpuBytes =
    static_cast<uint32_t>(sizeof(PointLightArrayStd140));

static_assert(kPointLightUniformGpuBytes <= 65536u, "Point light UBO must fit typical UBO limit");

}  // namespace rendering
}  // namespace eyelib
