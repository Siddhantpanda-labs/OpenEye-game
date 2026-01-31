#include "LightingPanel.h"
#include "api/render/Light.h"
#include "eyelib/math/Types.h"
#include "imgui.h"
#include <cmath>

namespace eyelib {
namespace test {

void LightingPanel::Render(api::render::DirectionalLight& light) {
    if (!ImGui::Begin("Lighting", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    float dir[3] = { light.direction.x, light.direction.y, light.direction.z };
    if (ImGui::SliderFloat3("Direction", dir, -1.0f, 1.0f, "%.2f")) {
        math::Vec3 d(dir[0], dir[1], dir[2]);
        float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (len > 1e-6f) light.direction = d / len;
    }
    float col[3] = { light.color.x, light.color.y, light.color.z };
    if (ImGui::ColorEdit3("Color", col))
        light.color = math::Vec3(col[0], col[1], col[2]);
    ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 5.0f, "%.2f");
    float amb[3] = { light.ambient.x, light.ambient.y, light.ambient.z };
    if (ImGui::ColorEdit3("Ambient", amb))
        light.ambient = math::Vec3(amb[0], amb[1], amb[2]);
    ImGui::SliderFloat("Exposure", &light.exposure, 0.1f, 3.0f, "%.2f");
    ImGui::Separator();
    ImGui::Text("Shadows: enabled in Deferred pipeline.");
    ImGui::End();
}

} // namespace test
} // namespace eyelib
