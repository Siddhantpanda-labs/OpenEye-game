#pragma once

#include "eyelib/fluids/FluidSimulation.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace fluids {

enum class FluidSliceAxis { X, Y, Z };

struct FluidSliceDesc {
    FluidSliceAxis axis = FluidSliceAxis::Z;
    float slice01 = 0.5f;
    float densityScale = 1.0f;
    bool visualizeTemperature = false;
};

/**
 * @brief CPU renderer for fluid debug/visualization. Produces RGBA8 slices.
 */
class FluidRenderer {
public:
    std::vector<uint8_t> BuildSliceRGBA8(const FluidSimulation& sim, const FluidSliceDesc& desc,
                                         int width, int height) const;
};

} // namespace fluids
} // namespace eyelib
