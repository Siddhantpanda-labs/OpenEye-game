#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace api {
namespace render {

/**
 * @brief Light API. Directional, point, spot lights.
 */
struct DirectionalLight {
    math::Vec3 direction{0.5f, 1.0f, 0.3f};
    math::Vec3 color{1.0f, 0.95f, 0.9f};
    float intensity = 3.0f;
    math::Vec3 ambient{0.08f};
    float exposure = 1.0f;
};

struct PointLight {
    math::Vec3 position{0, 0, 0};
    math::Vec3 color{1, 1, 1};
    float intensity = 1.0f;
    float radius = 10.0f;
};

struct SpotLight {
    math::Vec3 position{0, 0, 0};
    math::Vec3 direction{0, -1, 0};
    math::Vec3 color{1, 1, 1};
    float intensity = 1.0f;
    float range = 10.0f;
    float innerConeCos = 0.9f;  // cos(inner angle)
    float outerConeCos = 0.7f;  // cos(outer angle)
};

} // namespace render
} // namespace api
} // namespace eyelib
