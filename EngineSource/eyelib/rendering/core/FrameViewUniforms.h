#pragma once

#include "eyelib/math/Types.h"

#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace rendering {

/** GPU minimum practical UBO size for dynamic offset alignment (matches buffer allocation). */
inline constexpr uint32_t kPerFrameUniformGpuBytes = 256u;
inline constexpr uint32_t kPerViewUniformGpuBytes = 256u;

/** std140 — binding 0 in Frame set (`PerFrame`: frame index + delta time). */
struct PerFrameUniformStd140 {
    uint32_t frameIndex = 0;
    float deltaTime = 0.f;
    uint32_t _pad0 = 0;
    uint32_t _pad1 = 0;
};
static_assert(sizeof(PerFrameUniformStd140) == 16, "PerFrameUniformStd140 must stay std140-packed");

/**
 * std140 — binding 1 in Frame set (`PerView`: camera + viewport).
 * Padded to 256 bytes for `kPerViewUniformGpuBytes` buffer range.
 */
struct PerViewUniformStd140 {
    math::Mat4 view{1.f};
    math::Mat4 projection{1.f};
    math::Mat4 viewProjection{1.f};
    math::Vec4 viewPosition{0.f, 0.f, 0.f, 1.f};
    math::Vec4 viewport{1.f, 1.f, 1.f, 1.f};  // xy = width/height, zw = 1/w, 1/h
    uint8_t _padding[32]{};
};
static_assert(sizeof(PerViewUniformStd140) == 256, "PerViewUniformStd140 must match kPerViewUniformGpuBytes");

}  // namespace rendering
}  // namespace eyelib
