#include "InspectorPanel.h"
#include "../utils/SelectionManager.h"
#include "ecs/api/All.h"
#include "rendering/api/Camera.h"
#include "imgui.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace OpenMeta {
namespace Editor {

InspectorPanel::InspectorPanel(OpenEye::ECS::Scene* scene) 
    : m_scene(scene) {
}

void InspectorPanel::Render() {
    if (!m_isOpen || !m_scene) return;
    
    ImGui::Begin("Inspector", &m_isOpen);
    
    auto& selectionManager = SelectionManager::GetInstance();
    auto& registry = m_scene->GetRegistry();
    
    if (!selectionManager.HasSelection()) {
        ImGui::Text("No entity selected");
        ImGui::End();
        return;
    }
    
    OpenEye::ECS::Entity entity = selectionManager.GetSelectedEntity();
    
    // Render entity name
    if (registry.all_of<OpenEye::ECS::Name>(entity)) {
        RenderName(entity);
    }
    
    ImGui::Separator();
    
    // Render components
    if (registry.all_of<OpenEye::ECS::Transform>(entity)) {
        RenderTransform(entity);
    }
    
    if (registry.all_of<OpenEye::ECS::CameraComponent>(entity)) {
        RenderCamera(entity);
    }
    
    if (registry.all_of<OpenEye::ECS::Light>(entity)) {
        RenderLight(entity);
    }
    
    if (registry.all_of<OpenEye::ECS::Renderable>(entity)) {
        RenderRenderable(entity);
    }
    
    if (registry.all_of<OpenEye::ECS::AnimationComponent>(entity)) {
        RenderAnimation(entity);
    }
    
    if (registry.all_of<OpenEye::ECS::PhysicsBody>(entity)) {
        RenderPhysics(entity);
    }
    
    ImGui::Separator();
    
    // Add component button
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }
    
    if (ImGui::BeginPopup("AddComponentPopup")) {
        RenderAddComponentMenu(entity);
        ImGui::EndPopup();
    }
    
    ImGui::End();
}

void InspectorPanel::RenderName(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& name = registry.get<OpenEye::ECS::Name>(entity);
    
    char buffer[256];
    strncpy_s(buffer, name.name.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
        name.name = buffer;
    }
}

void InspectorPanel::RenderTransform(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& transform = registry.get<OpenEye::ECS::Transform>(entity);
    
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Position
        glm::vec3 pos = transform.position;
        if (ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f)) {
            transform.position = pos;
            transform.dirty = true;
        }
        
        // Rotation (as Euler angles for editing)
        glm::vec3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
        if (ImGui::DragFloat3("Rotation", glm::value_ptr(euler), 1.0f)) {
            transform.rotation = glm::quat(glm::radians(euler));
            transform.dirty = true;
        }
        
        // Scale
        glm::vec3 scale = transform.scale;
        if (ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f, 0.01f, 100.0f)) {
            transform.scale = scale;
            transform.dirty = true;
        }
    }
}

void InspectorPanel::RenderCamera(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& cameraComp = registry.get<OpenEye::ECS::CameraComponent>(entity);
    
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool isPrimary = cameraComp.isPrimary;
        if (ImGui::Checkbox("Primary Camera", &isPrimary)) {
            // If setting as primary, unset others
            if (isPrimary) {
                auto cameraView = registry.view<OpenEye::ECS::CameraComponent>();
                for (auto cam : cameraView) {
                    cameraView.get<OpenEye::ECS::CameraComponent>(cam).isPrimary = false;
                }
            }
            cameraComp.isPrimary = isPrimary;
        }
        
        // Edit CameraComponent fields directly (ECS controls the data)
        auto& camera = cameraComp.camera;
        
        float fovDegrees = glm::degrees(cameraComp.fov);
        if (ImGui::DragFloat("FOV", &fovDegrees, 1.0f, 10.0f, 170.0f)) {
            cameraComp.fov = glm::radians(fovDegrees);
            camera.SetPerspective(cameraComp.fov, cameraComp.aspect, cameraComp.nearPlane, cameraComp.farPlane);
        }
        
        if (ImGui::DragFloat("Near Plane", &cameraComp.nearPlane, 0.01f, 0.01f, 10.0f)) {
            camera.SetPerspective(cameraComp.fov, cameraComp.aspect, cameraComp.nearPlane, cameraComp.farPlane);
        }
        
        if (ImGui::DragFloat("Far Plane", &cameraComp.farPlane, 1.0f, 10.0f, 10000.0f)) {
            camera.SetPerspective(cameraComp.fov, cameraComp.aspect, cameraComp.nearPlane, cameraComp.farPlane);
        }
        
        if (ImGui::DragFloat("Aspect", &cameraComp.aspect, 0.01f, 0.1f, 10.0f)) {
            camera.SetPerspective(cameraComp.fov, cameraComp.aspect, cameraComp.nearPlane, cameraComp.farPlane);
        }
        
        if (ImGui::Button("Remove Component")) {
            registry.remove<OpenEye::ECS::CameraComponent>(entity);
        }
    }
}

void InspectorPanel::RenderLight(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& light = registry.get<OpenEye::ECS::Light>(entity);
    
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* types[] = { "Directional", "Point", "Spot" };
        int currentType = static_cast<int>(light.type);
        if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) {
            light.type = static_cast<OpenEye::ECS::Light::Type>(currentType);
        }
        
        ImGui::ColorEdit3("Color", glm::value_ptr(light.color));
        ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);
        
        if (light.type != OpenEye::ECS::Light::Type::Directional) {
            ImGui::DragFloat("Range", &light.range, 0.1f, 0.1f, 1000.0f);
        }
        
        if (light.type == OpenEye::ECS::Light::Type::Spot) {
            ImGui::DragFloat("Inner Cone", &light.innerConeAngle, 1.0f, 0.0f, light.outerConeAngle);
            ImGui::DragFloat("Outer Cone", &light.outerConeAngle, 1.0f, light.innerConeAngle, 180.0f);
        }
        
        ImGui::Checkbox("Cast Shadows", &light.castShadows);
        
        if (ImGui::Button("Remove Component")) {
            registry.remove<OpenEye::ECS::Light>(entity);
        }
    }
}

void InspectorPanel::RenderRenderable(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& renderable = registry.get<OpenEye::ECS::Renderable>(entity);
    
    if (ImGui::CollapsingHeader("Renderable", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Visible", &renderable.visible);
        
        // Mesh info
        if (renderable.mesh) {
            ImGui::Text("Mesh: <assigned>");
            ImGui::Text("Vertices: %zu", renderable.mesh->GetVertexCount());
            ImGui::Text("Indices: %zu", renderable.mesh->GetIndexCount());
        } else {
            ImGui::Text("No mesh assigned");
        }
        
        // Material info
        if (renderable.material) {
            ImGui::Text("Material: <assigned>");
        } else {
            ImGui::Text("No material assigned");
        }
        
        if (ImGui::Button("Remove Component")) {
            registry.remove<OpenEye::ECS::Renderable>(entity);
        }
    }
}

void InspectorPanel::RenderAnimation(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& anim = registry.get<OpenEye::ECS::AnimationComponent>(entity);
    
    if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Clips: %zu", anim.clips.size());
        ImGui::Text("Current Clip: %d", anim.currentClipIndex);
        ImGui::Text("Playing: %s", anim.isPlaying ? "Yes" : "No");
        ImGui::DragFloat("Playback Speed", &anim.playbackSpeed, 0.1f, 0.0f, 5.0f);
        ImGui::Checkbox("Loop", &anim.loop);
        
        if (ImGui::Button("Remove Component")) {
            registry.remove<OpenEye::ECS::AnimationComponent>(entity);
        }
    }
}

void InspectorPanel::RenderPhysics(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& physics = registry.get<OpenEye::ECS::PhysicsBody>(entity);
    
    if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Mass", &physics.mass, 0.1f, 0.0f, 10000.0f);
        ImGui::Checkbox("Kinematic", &physics.isKinematic);
        ImGui::Checkbox("Sync Transform -> Physics", &physics.syncTransformToPhysics);
        ImGui::Checkbox("Sync Physics -> Transform", &physics.syncPhysicsToTransform);
        
        if (ImGui::Button("Remove Component")) {
            registry.remove<OpenEye::ECS::PhysicsBody>(entity);
        }
    }
}

void InspectorPanel::RenderAddComponentMenu(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    
    if (!registry.all_of<OpenEye::ECS::CameraComponent>(entity)) {
        if (ImGui::MenuItem("Camera")) {
            registry.emplace<OpenEye::ECS::CameraComponent>(entity);
        }
    }
    
    if (!registry.all_of<OpenEye::ECS::Light>(entity)) {
        if (ImGui::MenuItem("Light")) {
            registry.emplace<OpenEye::ECS::Light>(entity);
        }
    }
    
    if (!registry.all_of<OpenEye::ECS::Renderable>(entity)) {
        if (ImGui::MenuItem("Renderable")) {
            registry.emplace<OpenEye::ECS::Renderable>(entity);
        }
    }
    
    if (!registry.all_of<OpenEye::ECS::AnimationComponent>(entity)) {
        if (ImGui::MenuItem("Animation")) {
            registry.emplace<OpenEye::ECS::AnimationComponent>(entity);
        }
    }
    
    if (!registry.all_of<OpenEye::ECS::PhysicsBody>(entity)) {
        if (ImGui::MenuItem("Physics Body")) {
            registry.emplace<OpenEye::ECS::PhysicsBody>(entity);
        }
    }
}

} // namespace Editor
} // namespace OpenMeta

