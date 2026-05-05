#pragma once

#include <cstdint>
#include <vector>

namespace eyelib {
namespace rendering {

/**
 * CPU-side pixel data for bootstrap IBL (used when no environment capture exists).
 * Lives under `lighting/ibl/` with other IBL concerns; output matches `IDevice::CreateTexture` upload rules.
 */
namespace ibl_cpu {

/**
 * CPU fallback BRDF LUT builder.
 * @param dimension output size (dimension x dimension).
 * @param sampleCount GGX integration samples per texel (bounded fallback path should keep this modest).
 */
void BuildBrdfLutRgba16F(std::vector<uint8_t>& outRgba16f, uint32_t dimension, uint32_t sampleCount = 128u);
void BuildNeutralSkylightCubemapRgba16F(std::vector<uint8_t>& outRgba16f, uint32_t faceSize);

} // namespace ibl_cpu

} // namespace rendering
} // namespace eyelib
