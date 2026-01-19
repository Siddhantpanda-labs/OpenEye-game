#include "LightingPanel.h"
#include "ecs/api/All.h"
#include "imgui.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace OpenMeta {
namespace Editor {

LightingPanel::LightingPanel(OpenEye::ECS::Scene* scene, OpenEye::Rendering::RenderPipeline* renderPipeline)
    : m_scene(scene), m_renderPipeline(renderPipeline) {
}

void LightingPanel::Render() {
    if (!m_isOpen || !m_scene) return;
    
    ImGui::Begin("Lighting", &m_isOpen);
    
    auto& registry = m_scene->GetRegistry();
    
    // Sky Settings
    RenderSkySettings();
    
    ImGui::Separator();
    
    // Collect all lights
    auto lightView = registry.view<OpenEye::ECS::Light, OpenEye::ECS::Transform>();
    std::vector<OpenEye::ECS::Entity> lights;
    for (auto entity : lightView) {
        lights.push_back(entity);
    }
    
    // Light List
    ImGui::Text("Lights (%zu)", lights.size());
    
    if (ImGui::Button("Add Light")) {
        ImGui::OpenPopup("AddLightPopup");
    }
    
    if (ImGui::BeginPopup("AddLightPopup")) {
        if (ImGui::MenuItem("Directional Light")) {
            auto lightEntity = m_scene->CreateEntity("Directional Light");
            auto& light = registry.emplace<OpenEye::ECS::Light>(lightEntity);
            light.type = OpenEye::ECS::Light::Type::Directional;
            light.color = glm::vec3(1.0f);
            light.intensity = 1.0f;
            auto& transform = registry.get<OpenEye::ECS::Transform>(lightEntity);
            transform.rotation = glm::quatLookAt(glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f)), glm::vec3(0.0f, 1.0f, 0.0f));
        }
        if (ImGui::MenuItem("Spot Light")) {
            auto lightEntity = m_scene->CreateEntity("Spot Light");
            auto& light = registry.emplace<OpenEye::ECS::Light>(lightEntity);
            light.type = OpenEye::ECS::Light::Type::Spot;
            light.color = glm::vec3(1.0f, 1.0f, 0.9f);
            light.intensity = 2.0f;
            light.range = 10.0f;
            light.innerConeAngle = 15.0f;
            light.outerConeAngle = 30.0f;
            auto& transform = registry.get<OpenEye::ECS::Transform>(lightEntity);
            transform.position = glm::vec3(0.0f, 5.0f, 0.0f);
            transform.rotation = glm::quatLookAt(glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f)), glm::vec3(0.0f, 1.0f, 0.0f));
        }
        if (ImGui::MenuItem("Point Light")) {
            auto lightEntity = m_scene->CreateEntity("Point Light");
            auto& light = registry.emplace<OpenEye::ECS::Light>(lightEntity);
            light.type = OpenEye::ECS::Light::Type::Point;
            light.color = glm::vec3(1.0f, 0.8f, 0.6f);
            light.intensity = 5.0f;
            light.range = 10.0f;
            auto& transform = registry.get<OpenEye::ECS::Transform>(lightEntity);
            transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
        }
        ImGui::EndPopup();
    }
    
    ImGui::Separator();
    
    // Display and edit lights
    for (size_t i = 0; i < lights.size(); ++i) {
        auto entity = lights[i];
        auto& light = lightView.get<OpenEye::ECS::Light>(entity);
        auto& transform = lightView.get<OpenEye::ECS::Transform>(entity);
        
        const char* typeNames[] = { "Directional", "Point", "Spot" };
        const char* typeName = typeNames[static_cast<int>(light.type)];
        
        bool isSelected = (m_selectedLightIndex == static_cast<int>(i));
        if (ImGui::Selectable(std::string(typeName).append("##").append(std::to_string(i)).c_str(), isSelected)) {
            m_selectedLightIndex = static_cast<int>(i);
        }
        
        if (isSelected) {
            ImGui::Indent();
            
            // Light Type
            int currentType = static_cast<int>(light.type);
            if (ImGui::Combo("Type", &currentType, typeNames, 3)) {
                light.type = static_cast<OpenEye::ECS::Light::Type>(currentType);
            }
            
            // Color
            ImGui::ColorEdit3("Color", glm::value_ptr(light.color));
            
            // Intensity
            ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);
            
            // Position (for point/spot lights)
            if (light.type != OpenEye::ECS::Light::Type::Directional) {
                if (ImGui::DragFloat3("Position", glm::value_ptr(transform.position), 0.1f)) {
                    transform.dirty = true;
                }
                ImGui::DragFloat("Range", &light.range, 0.1f, 0.1f, 1000.0f);
            }
            
            // Direction (for directional/spot lights)
            if (light.type != OpenEye::ECS::Light::Type::Point) {
                glm::mat4 worldMatrix = transform.GetWorldMatrix(registry);
                glm::mat3 rotationMatrix = glm::mat3(worldMatrix);
                glm::vec3 forward = glm::normalize(rotationMatrix * glm::vec3(0.0f, 0.0f, -1.0f));
                glm::vec3 direction = forward;
                
                if (ImGui::DragFloat3("Direction", glm::value_ptr(direction), 0.01f, -1.0f, 1.0f)) {
                    direction = glm::normalize(direction);
                    transform.rotation = glm::quatLookAt(direction, glm::vec3(0.0f, 1.0f, 0.0f));
                    transform.dirty = true;
                }
            }
            
            // Spot light specific
            if (light.type == OpenEye::ECS::Light::Type::Spot) {
                ImGui::DragFloat("Inner Cone", &light.innerConeAngle, 1.0f, 0.0f, light.outerConeAngle);
                ImGui::DragFloat("Outer Cone", &light.outerConeAngle, 1.0f, light.innerConeAngle, 180.0f);
            }
            
            // Shadows
            ImGui::Checkbox("Cast Shadows", &light.castShadows);
            
            // Delete button
            if (ImGui::Button("Delete Light")) {
                registry.destroy(entity);
                lights.erase(lights.begin() + i);
                m_selectedLightIndex = -1;
                break;
            }
            
            ImGui::Unindent();
        }
    }
    
    ImGui::End();
}

void LightingPanel::RenderSkySettings() {
    if (!m_renderPipeline) return;
    
    if (ImGui::CollapsingHeader("Sky Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        glm::vec3 skyColor = m_renderPipeline->GetSkyColor();
        if (ImGui::ColorEdit3("Sky Color", glm::value_ptr(skyColor))) {
            m_renderPipeline->SetSkyColor(skyColor);
        }
    }
}

} // namespace Editor
} // namespace OpenMeta
