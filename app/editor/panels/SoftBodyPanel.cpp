#include "SoftBodyPanel.h"
#include "core/log/Log.h"
#include "imgui.h"
#include <glm/gtc/type_ptr.hpp>

using namespace OpenEye::Core;
using namespace OpenEye::ECS;

namespace OpenMeta {
namespace Editor {

SoftBodyPanel::SoftBodyPanel(OpenEye::ECS::Scene* scene) : m_scene(scene) {}

void SoftBodyPanel::Render() {
    if (!m_scene) return;
    
    ImGui::Begin("Soft Bodies", &m_isOpen);
    
    auto& registry = m_scene->GetRegistry();
    
    // List all soft bodies
    auto softBodyView = registry.view<SoftBodyComponent>();
    
    if (softBodyView.empty()) {
        ImGui::Text("No soft bodies in scene");
        
        if (ImGui::Button("Create Cloth")) {
            // Create a new entity with cloth
            auto entity = m_scene->CreateEntity("Cloth");
            auto& transform = registry.get<Transform>(entity);
            transform.position = glm::vec3(0.0f, 4.0f, -2.0f);
            
            // Get physics world
            // TODO: Get physics world from scene or game application
            ImGui::Text("Physics world access needed");
        }
    } else {
        ImGui::Text("Soft Bodies: %zu", softBodyView.size());
        ImGui::Separator();
        
        for (auto entity : softBodyView) {
            if (!registry.valid(entity)) continue;
            auto& softBodyComp = registry.get<SoftBodyComponent>(entity);
            
            // Get entity name
            std::string entityName = "Entity";
            if (registry.all_of<Name>(entity)) {
                entityName = registry.get<Name>(entity).name;
            }
            
            ImGui::PushID(static_cast<int>(entity));
            
            if (ImGui::CollapsingHeader(entityName.c_str())) {
                RenderSoftBody(entity, softBodyComp);
            }
            
            ImGui::PopID();
        }
    }
    
    ImGui::End();
}

void SoftBodyPanel::RenderSoftBody(OpenEye::ECS::Entity entity, SoftBodyComponent& softBodyComp) {
    auto& registry = m_scene->GetRegistry();
    
    // Enabled checkbox
    ImGui::Checkbox("Enabled", &softBodyComp.enabled);
    
    // Soft body info
    if (softBodyComp.softBody) {
        ImGui::Text("Soft Body: Present");
        
        // Mesh info
        if (softBodyComp.mesh) {
            ImGui::Text("Mesh Vertices: %zu", softBodyComp.mesh->GetVertexCount());
            ImGui::Text("Mesh Indices: %zu", softBodyComp.mesh->GetIndexCount());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No mesh!");
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No soft body!");
    }
    
    ImGui::Separator();
    
    // Wind settings
    ImGui::Checkbox("Wind Enabled", &softBodyComp.windEnabled);
    if (softBodyComp.windEnabled) {
        glm::vec3 dir = softBodyComp.windDirection;
        float strength = softBodyComp.windStrength;

        bool dirChanged = ImGui::DragFloat3("Wind Direction", glm::value_ptr(dir), 0.05f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("World-space direction. Will be normalized (avoid 0,0,0).");
        }

        bool strengthChanged = ImGui::SliderFloat("Wind Strength", &strength, 0.0f, 25.0f, "%.2f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Higher values = stronger wind. 0 disables effect.");
        }

        if (dirChanged) {
            // Prevent NaNs / Bullet instability: don't allow zero direction.
            const float len = glm::length(dir);
            if (len < 1e-4f) {
                dir = glm::vec3(1.0f, 0.0f, 0.0f);
            } else {
                dir /= len;
            }
            softBodyComp.windDirection = dir;
        }
        if (strengthChanged) {
            softBodyComp.windStrength = glm::clamp(strength, 0.0f, 25.0f);
        }
    }
    
    ImGui::Separator();
    
    // Presets
    if (ImGui::Button("Default Cloth")) {
        ApplyClothPreset(entity, "default");
    }
    ImGui::SameLine();
    if (ImGui::Button("Flag Cloth")) {
        ApplyClothPreset(entity, "flag");
    }
    ImGui::SameLine();
    if (ImGui::Button("Curtain Cloth")) {
        ApplyClothPreset(entity, "curtain");
    }
}

void SoftBodyPanel::ApplyClothPreset(OpenEye::ECS::Entity entity, const char* presetName) {
    // TODO: Implement cloth preset creation
    // This requires access to GameApplication's physics world
    OE_WARN("SoftBodyPanel: Cloth preset creation not yet implemented in editor");
}

} // namespace Editor
} // namespace OpenMeta
