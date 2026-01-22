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

    // --------------------------------------------------------------------
    // XPBD Cloth (CPU)
    // --------------------------------------------------------------------
    if (ImGui::CollapsingHeader("XPBD Cloth (CPU)", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto clothView = registry.view<XPBDClothComponent>();
        if (clothView.empty()) {
            ImGui::Text("No XPBD cloth entities in scene");
        } else {
            ImGui::Text("Cloth Entities: %zu", clothView.size());
            for (auto entity : clothView) {
                if (!registry.valid(entity)) continue;
                auto& cloth = registry.get<XPBDClothComponent>(entity);

                std::string name = "XPBDCloth";
                if (registry.all_of<Name>(entity)) name = registry.get<Name>(entity).name;

                ImGui::PushID(static_cast<int>(entity));
                if (ImGui::TreeNode(name.c_str())) {
                    ImGui::Checkbox("Enabled", &cloth.enabled);

                    ImGui::Separator();
                    ImGui::Text("Simulation");
                    ImGui::SliderInt("Iterations", &cloth.iterations, 1, 30);
                    ImGui::Checkbox("Shear Constraints", &cloth.enableShear);
                    ImGui::SliderFloat("Compliance (stiffness)", &cloth.compliance, 0.0f, 0.01f, "%.6f", ImGuiSliderFlags_Logarithmic);
                    ImGui::SliderFloat("Damping", &cloth.damping, 0.0f, 0.2f, "%.3f");

                    ImGui::Separator();
                    ImGui::Text("Wind");
                    ImGui::Checkbox("Wind Enabled", &cloth.windEnabled);
                    if (cloth.windEnabled) {
                        glm::vec3 dir = cloth.windDirection;
                        float strength = cloth.windStrength;
                        bool dirChanged = ImGui::DragFloat3("Wind Direction", glm::value_ptr(dir), 0.05f);
                        bool strengthChanged = ImGui::SliderFloat("Wind Strength", &strength, 0.0f, 30.0f, "%.2f");
                        if (dirChanged) {
                            float len = glm::length(dir);
                            if (len < 1e-4f) dir = glm::vec3(1, 0, 0);
                            else dir /= len;
                            cloth.windDirection = dir;
                        }
                        if (strengthChanged) cloth.windStrength = glm::clamp(strength, 0.0f, 30.0f);
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }

    ImGui::Separator();

    // --------------------------------------------------------------------
    // Jelly Blob (rigid cluster + deforming mesh)
    // --------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Jelly Blob (Rigid Cluster)", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto jellyView = registry.view<JellyBlobComponent>();
        if (jellyView.empty()) {
            ImGui::Text("No jelly blobs in scene");
        } else {
            ImGui::Text("Jelly Blobs: %zu", jellyView.size());
            for (auto entity : jellyView) {
                if (!registry.valid(entity)) continue;
                auto& jelly = registry.get<JellyBlobComponent>(entity);

                std::string name = "JellyBlob";
                if (registry.all_of<Name>(entity)) name = registry.get<Name>(entity).name;

                ImGui::PushID(static_cast<int>(entity));
                if (ImGui::TreeNode(name.c_str())) {
                    ImGui::Checkbox("Enabled", &jelly.enabled);
                    ImGui::Checkbox("Show Debug Nodes", &jelly.showDebugNodes);
                    ImGui::Text("Nodes: %zu", jelly.nodeEntities.size());
                    ImGui::Text("Constraints: %zu", jelly.constraints.size());

                    ImGui::Separator();
                    ImGui::Text("Springs");
                    ImGui::SliderFloat("Spring Tau", &jelly.springTau, 0.0f, 1.0f, "%.3f");
                    ImGui::SliderFloat("Spring Damping", &jelly.springDamping, 0.0f, 1.0f, "%.3f");

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }
    
    // List all soft bodies
    auto softBodyView = registry.view<SoftBodyComponent>();
    
    if (softBodyView.empty()) {
        ImGui::Text("No Bullet soft bodies in scene");
        
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
        ImGui::Separator();
        ImGui::Text("Bullet Soft Bodies (Legacy): %zu", softBodyView.size());
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
    
    // Impulse / pulse controls (good for volumetric softbodies)
    ImGui::Text("Impulse / Pulse");
    {
        glm::vec3 dir = softBodyComp.impulseDirection;
        float strength = softBodyComp.impulseStrength;
        bool pulseEnabled = softBodyComp.pulseEnabled;
        float pulseInterval = softBodyComp.pulseInterval;

        bool dirChanged = ImGui::DragFloat3("Impulse Direction", glm::value_ptr(dir), 0.05f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Normalized. Avoid 0,0,0.");

        bool strengthChanged = ImGui::SliderFloat("Impulse Strength", &strength, 0.0f, 25.0f, "%.2f");
        bool pulseChanged = ImGui::Checkbox("Pulse Enabled", &pulseEnabled);
        bool intervalChanged = false;
        if (pulseEnabled) {
            intervalChanged = ImGui::SliderFloat("Pulse Interval (sec)", &pulseInterval, 0.1f, 5.0f, "%.2f");
        }

        if (ImGui::Button("Apply Impulse Now")) {
            softBodyComp.impulseRequested = true;
        }

        if (dirChanged) {
            const float len = glm::length(dir);
            if (len < 1e-4f) dir = glm::vec3(0.0f, 1.0f, 0.0f);
            else dir /= len;
            softBodyComp.impulseDirection = dir;
        }
        if (strengthChanged) softBodyComp.impulseStrength = glm::clamp(strength, 0.0f, 25.0f);
        if (pulseChanged) softBodyComp.pulseEnabled = pulseEnabled;
        if (intervalChanged) softBodyComp.pulseInterval = glm::clamp(pulseInterval, 0.1f, 5.0f);
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
