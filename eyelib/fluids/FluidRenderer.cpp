#include "eyelib/fluids/FluidRenderer.h"

#include <algorithm>

namespace eyelib {
namespace fluids {

namespace {
static uint8_t ToByte(float v) {
    const float c = std::max(0.0f, std::min(1.0f, v));
    return static_cast<uint8_t>(c * 255.0f + 0.5f);
}

static math::Vec4 TemperatureColor(float t) {
    // blue -> cyan -> yellow -> red
    t = std::max(0.0f, std::min(1.0f, t));
    if (t < 0.33f) {
        const float k = t / 0.33f;
        return math::Vec4(0.0f, k, 1.0f, 1.0f);
    }
    if (t < 0.66f) {
        const float k = (t - 0.33f) / 0.33f;
        return math::Vec4(k, 1.0f, 1.0f - k, 1.0f);
    }
    const float k = (t - 0.66f) / 0.34f;
    return math::Vec4(1.0f, 1.0f - k, 0.0f, 1.0f);
}
} // namespace

std::vector<uint8_t> FluidRenderer::BuildSliceRGBA8(const FluidSimulation& sim, const FluidSliceDesc& desc,
                                                    int width, int height) const {
    if (width <= 0 || height <= 0) return {};
    std::vector<uint8_t> out(static_cast<size_t>(width * height * 4), 0);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float u = static_cast<float>(x) / std::max(1, width - 1);
            const float v = static_cast<float>(y) / std::max(1, height - 1);
            math::Vec3 p01(0.0f);
            switch (desc.axis) {
                case FluidSliceAxis::X: p01 = math::Vec3(desc.slice01, v, u); break;
                case FluidSliceAxis::Y: p01 = math::Vec3(u, desc.slice01, v); break;
                case FluidSliceAxis::Z: p01 = math::Vec3(u, v, desc.slice01); break;
            }
            const float d = sim.SampleDensity(p01) * desc.densityScale;
            const float t = sim.SampleTemperature(p01);

            math::Vec4 color;
            if (desc.visualizeTemperature) color = TemperatureColor(t);
            else color = math::Vec4(1.0f, 1.0f, 1.0f, d);

            const size_t idx = static_cast<size_t>((y * width + x) * 4);
            out[idx + 0] = ToByte(color.x);
            out[idx + 1] = ToByte(color.y);
            out[idx + 2] = ToByte(color.z);
            out[idx + 3] = ToByte(color.w);
        }
    }
    return out;
}

} // namespace fluids
} // namespace eyelib
