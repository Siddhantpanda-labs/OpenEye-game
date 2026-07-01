#include "eyelib/rendering/lighting/SceneLighting.h"

#include <algorithm>
#include <vector>
#include <glm/geometric.hpp>

namespace eyelib {
namespace rendering {

namespace {
DirectionalLightSceneState g_directionalLight{};
std::vector<SceneLighting::SpotLightSceneState> g_SpotLights;
std::vector<SceneLighting::PointLightSceneState> g_PointLights;
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
 
void SceneLighting::SetIblIntensity(float intensity) noexcept {
    g_directionalLight.iblIntensity = (std::max)(intensity, 0.0f);
}

void SceneLighting::ClearSpotLights() noexcept {
    g_SpotLights.clear();
}

void SceneLighting::AddSpotLight(const SpotLightSceneState& spotLight) noexcept {
    if (g_SpotLights.size() < 16) { // hard limit for now
        g_SpotLights.push_back(spotLight);
    }
}

const SceneLighting::SpotLightSceneState* SceneLighting::GetSpotLights(uint32_t& outCount) noexcept {
    outCount = static_cast<uint32_t>(g_SpotLights.size());
    return g_SpotLights.data();
}

void SceneLighting::ClearPointLights() noexcept {
    g_PointLights.clear();
}

void SceneLighting::AddPointLight(const PointLightSceneState& pointLight) noexcept {
    if (g_PointLights.size() < SceneLighting::kMaxScenePointLights) {
        g_PointLights.push_back(pointLight);
    }
}

const SceneLighting::PointLightSceneState* SceneLighting::GetPointLights(uint32_t& outCount) noexcept {
    outCount = static_cast<uint32_t>(g_PointLights.size());
    return g_PointLights.data();
}

}  // namespace rendering
}  // namespace eyelib
