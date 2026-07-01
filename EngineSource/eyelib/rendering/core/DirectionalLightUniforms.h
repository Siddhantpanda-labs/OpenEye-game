#pragma once

#include "eyelib/math/Types.h"

#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace rendering {

inline constexpr uint32_t kDirectionalLightUniformGpuBytes = 256u;

/** std140 — binding 1 in `RasterForwardData` set (single directional light). */
struct DirectionalLightStd140 {
    math::Vec4 direction{0.35f, -0.85f, 0.25f, 0.f};
    math::Vec4 colorIntensity{1.f, 0.97f, 0.92f, 3.5f};
    math::Vec4 iblParams{1.f, 0.f, 0.f, 0.f}; // x = intensity
    uint8_t _padding[kDirectionalLightUniformGpuBytes - sizeof(math::Vec4) * 3]{};
};
static_assert(sizeof(DirectionalLightStd140) == kDirectionalLightUniformGpuBytes, "DirectionalLightStd140 must match GPU UBO size");

}  // namespace rendering
}  // namespace eyelib
