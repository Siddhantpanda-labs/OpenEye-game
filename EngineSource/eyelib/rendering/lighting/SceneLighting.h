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

    struct SpotLightSceneState {
        math::Vec3 position = {0.0f, 5.0f, 0.0f};
        float range = 20.0f;
        math::Vec3 direction = {0.0f, -1.0f, 0.0f};
        float intensity = 10.0f;
        math::Vec3 color = {1.0f, 1.0f, 1.0f};
        float innerConeCos = 0.9f;
        float outerConeCos = 0.8f;
        bool castShadows = true;
    };

    static void ClearSpotLights() noexcept;
    static void AddSpotLight(const SpotLightSceneState& spotLight) noexcept;
    static const SpotLightSceneState* GetSpotLights(uint32_t& outCount) noexcept;
};
}  // namespace rendering
}  // namespace eyelib
