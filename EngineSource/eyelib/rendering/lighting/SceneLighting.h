#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace rendering {

struct DirectionalLightSceneState {
    math::Vec3 direction = {-0.35f, -0.85f, -0.25f};
    math::Vec3 color = {1.0f, 0.95f, 0.85f};
    float intensity = 5.0f;
};

class SceneLighting {
public:
    SceneLighting() = default;

    static void SetDirectionalLight(const math::Vec3& direction, const math::Vec3& color, float intensity) noexcept;
    static DirectionalLightSceneState GetDirectionalLight() noexcept;
};
}  // namespace rendering
}  // namespace eyelib
