#include "eyelib/rendering/lighting/SceneLighting.h"

#include <algorithm>
#include <glm/geometric.hpp>

namespace eyelib {
namespace rendering {

namespace {
DirectionalLightSceneState g_directionalLight{};
} // namespace

void SceneLighting::SetDirectionalLight(const math::Vec3& direction, const math::Vec3& color, float intensity) noexcept {
    const float len = glm::length(direction);
    if (len > 1e-5f) {
        g_directionalLight.direction = direction / len;
    }
    g_directionalLight.color = color;
    g_directionalLight.intensity = (std::max)(intensity, 0.0f);
}

DirectionalLightSceneState SceneLighting::GetDirectionalLight() noexcept {
    return g_directionalLight;
}

}  // namespace rendering
}  // namespace eyelib
