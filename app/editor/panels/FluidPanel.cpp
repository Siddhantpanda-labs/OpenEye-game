#include "FluidPanel.h"
#include "ecs/api/All.h"
#include "ecs/Components.h"
#include "fluids/FluidSimulation.h"
#include "fluids/FluidCommon.h"
#include "fluids/templates/FluidTemplates.h"
#include "imgui.h"
#include <glm/gtc/type_ptr.hpp>

namespace OpenMeta {
namespace Editor {

FluidPanel::FluidPanel(OpenEye::ECS::Scene* scene) : m_scene(scene) {}

void FluidPanel::Render() {
    if (!m_isOpen || !m_scene) return;
    ImGui::Begin("Fluids", &m_isOpen);
    
    auto& registry = m_scene->GetRegistry();
    auto fluidView = registry.view<OpenEye::ECS::FluidVolumeComponent, OpenEye::ECS::Transform>();
    std::vector<OpenEye::ECS::Entity> volumes;
    for (auto entity : fluidView) volumes.push_back(entity);
    ImGui::Text("Fluid Volumes (%zu)", volumes.size());
    if (ImGui::Button("Add Volume")) {
        auto fluidEntity = m_scene->CreateEntity("FluidVolume");
        auto& transform = registry.get<OpenEye::ECS::Transform>(fluidEntity);
        transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
        OpenEye::Fluids::FluidGridDesc desc;
        desc.resolution = glm::ivec3(64, 64, 64);
        desc.volumeSize = glm::vec3(2.0f, 2.0f, 2.0f);
        auto fluidSim = std::make_shared<OpenEye::Fluids::FluidSimulation>();
        if (fluidSim->Initialize(desc, OpenEye::Fluids::FluidType::Gas)) {
            auto& fluidComp = registry.emplace<OpenEye::ECS::FluidVolumeComponent>(fluidEntity);
            fluidComp.simulation = fluidSim;
            fluidComp.resolution = desc.resolution;
            fluidComp.volumeSize = desc.volumeSize;
            fluidComp.enabled = true;
        }
    }
    ImGui::SameLine();
    // Use a unique popup name for this panel to avoid ID collisions with other panels
    if (ImGui::Button("Presets")) ImGui::OpenPopup("FluidPresetPopup");
    if (ImGui::BeginPopup("FluidPresetPopup")) {
        if (ImGui::MenuItem("Fire (Smoke)")) { auto e = m_scene->CreateEntity("FireFluid"); ApplyFirePreset(e); }
        if (ImGui::MenuItem("Water")) { auto e = m_scene->CreateEntity("WaterFluid"); ApplyWaterPreset(e); }
        if (ImGui::MenuItem("Explosion")) { auto e = m_scene->CreateEntity("ExplosionFluid"); ApplyExplosionPreset(e); }
        ImGui::EndPopup();
    }
    ImGui::Separator();
    for (size_t i = 0; i < volumes.size(); i++) {
        auto entity = volumes[i];
        // Safety check: ensure entity still exists and has Name component
        if (!registry.valid(entity) || !registry.all_of<OpenEye::ECS::Name>(entity)) {
            continue;
        }
        auto& name = registry.get<OpenEye::ECS::Name>(entity);
        bool isSelected = (m_selectedVolume == entity);
        if (ImGui::Selectable(name.name.c_str(), isSelected)) m_selectedVolume = entity;
        ImGui::SameLine();
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton("X")) {
            if (m_selectedVolume == entity) m_selectedVolume = OpenEye::ECS::Entity{};
            m_scene->DestroyEntity(entity);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    // Safety check: validate selected entity exists and has required components before rendering
    if (m_selectedVolume != OpenEye::ECS::Entity{} && 
        registry.valid(m_selectedVolume) &&
        registry.all_of<OpenEye::ECS::FluidVolumeComponent, OpenEye::ECS::Transform>(m_selectedVolume)) {
        auto& fluidComp = registry.get<OpenEye::ECS::FluidVolumeComponent>(m_selectedVolume);
        RenderFluidVolume(m_selectedVolume, fluidComp);
    } else if (m_selectedVolume != OpenEye::ECS::Entity{}) {
        // Selected entity no longer exists or is invalid, clear selection
        m_selectedVolume = OpenEye::ECS::Entity{};
    }
    ImGui::End();
}

void FluidPanel::RenderFluidVolume(OpenEye::ECS::Entity entity, OpenEye::ECS::FluidVolumeComponent& fluidComp) {
    if (!fluidComp.simulation) return;
    auto& registry = m_scene->GetRegistry();
    // Safety check: ensure entity still exists and has Transform component
    if (!registry.valid(entity) || !registry.all_of<OpenEye::ECS::Transform>(entity)) {
        return;
    }
    auto& transform = registry.get<OpenEye::ECS::Transform>(entity);
    ImGui::Text("Fluid Volume Properties");
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", glm::value_ptr(transform.position), 0.1f);
        transform.dirty = true;
    }
    if (ImGui::CollapsingHeader("Volume Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled", &fluidComp.enabled);
        glm::ivec3 res = fluidComp.resolution;
        if (ImGui::DragInt3("Resolution", glm::value_ptr(res), 1.0f, 16, 256)) {
            if (res != fluidComp.resolution) {
                OpenEye::Fluids::FluidGridDesc desc;
                desc.resolution = res;
                desc.volumeSize = fluidComp.volumeSize;
                auto newSim = std::make_shared<OpenEye::Fluids::FluidSimulation>();
                if (newSim->Initialize(desc, fluidComp.simulation->GetType())) {
                    fluidComp.simulation = newSim;
                    fluidComp.resolution = res;
                }
            }
        }
        if (ImGui::DragFloat3("Volume Size", glm::value_ptr(fluidComp.volumeSize), 0.1f, 0.1f, 20.0f)) {
            OpenEye::Fluids::FluidGridDesc desc;
            desc.resolution = fluidComp.resolution;
            desc.volumeSize = fluidComp.volumeSize;
            auto newSim = std::make_shared<OpenEye::Fluids::FluidSimulation>();
            if (newSim->Initialize(desc, fluidComp.simulation->GetType())) {
                fluidComp.simulation = newSim;
            }
        }
    }
    if (ImGui::CollapsingHeader("Emission")) {
        glm::vec3 emitPos;
        float emitRadius, densityRate, tempRate;
        bool emitEnabled;
        fluidComp.simulation->GetEmission(emitPos, emitRadius, densityRate, tempRate, emitEnabled);
        if (ImGui::Checkbox("Emission Enabled", &emitEnabled)) {
            fluidComp.simulation->SetEmission(emitPos, emitRadius, densityRate, tempRate, emitEnabled);
        }
        if (ImGui::DragFloat3("Emission Position", glm::value_ptr(emitPos), 0.01f, 0.0f, 1.0f)) {
            fluidComp.simulation->SetEmission(emitPos, emitRadius, densityRate, tempRate, emitEnabled);
        }
        if (ImGui::DragFloat("Emission Radius", &emitRadius, 0.01f, 0.0f, 0.5f)) {
            fluidComp.simulation->SetEmission(emitPos, emitRadius, densityRate, tempRate, emitEnabled);
        }
        if (ImGui::DragFloat("Density Rate", &densityRate, 0.1f, 0.0f, 5.0f)) {
            fluidComp.simulation->SetEmission(emitPos, emitRadius, densityRate, tempRate, emitEnabled);
        }
        if (ImGui::DragFloat("Temperature Rate", &tempRate, 0.1f, 0.0f, 5.0f)) {
            fluidComp.simulation->SetEmission(emitPos, emitRadius, densityRate, tempRate, emitEnabled);
        }
    }
    if (ImGui::CollapsingHeader("Buoyancy")) {
        float sigma, kappa, ambient;
        fluidComp.simulation->GetBuoyancy(sigma, kappa, ambient);
        if (ImGui::DragFloat("Sigma (Buoyancy)", &sigma, 0.1f, 0.0f, 5.0f)) {
            fluidComp.simulation->SetBuoyancy(sigma, kappa, ambient);
        }
        if (ImGui::DragFloat("Kappa (Weight)", &kappa, 0.01f, 0.0f, 0.2f)) {
            fluidComp.simulation->SetBuoyancy(sigma, kappa, ambient);
        }
        if (ImGui::DragFloat("Ambient Temp", &ambient, 0.1f, -5.0f, 5.0f)) {
            fluidComp.simulation->SetBuoyancy(sigma, kappa, ambient);
        }
    }
    if (ImGui::CollapsingHeader("Wind")) {
        glm::vec3 windDir;
        float windStrength;
        bool windEnabled;
        fluidComp.simulation->GetWind(windDir, windStrength, windEnabled);
        if (ImGui::Checkbox("Wind Enabled", &windEnabled)) {
            fluidComp.simulation->SetWind(windDir, windStrength, windEnabled);
        }
        if (ImGui::DragFloat3("Wind Direction", glm::value_ptr(windDir), 0.01f, -1.0f, 1.0f)) {
            fluidComp.simulation->SetWind(windDir, windStrength, windEnabled);
        }
        if (ImGui::DragFloat("Wind Strength", &windStrength, 0.1f, 0.0f, 10.0f)) {
            fluidComp.simulation->SetWind(windDir, windStrength, windEnabled);
        }
    }
    if (ImGui::CollapsingHeader("Dissipation")) {
        float densityDiss = fluidComp.simulation->GetDensityDissipation();
        float velocityDiss = fluidComp.simulation->GetVelocityDissipation();
        if (ImGui::DragFloat("Density Dissipation", &densityDiss, 0.001f, 0.9f, 1.0f)) {
            fluidComp.simulation->SetDissipation(densityDiss, velocityDiss);
        }
        if (ImGui::DragFloat("Velocity Dissipation", &velocityDiss, 0.001f, 0.9f, 1.0f)) {
            fluidComp.simulation->SetDissipation(densityDiss, velocityDiss);
        }
    }
    if (ImGui::CollapsingHeader("Control")) {
        if (ImGui::Button("Clear Density")) fluidComp.simulation->ClearDensity();
        int jacobiIters = fluidComp.simulation->GetJacobiIterations();
        if (ImGui::DragInt("Jacobi Iterations", &jacobiIters, 1, 1, 100)) {
            fluidComp.simulation->SetJacobiIterations(jacobiIters);
        }
    }
}

void FluidPanel::ApplyFirePreset(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& transform = registry.get<OpenEye::ECS::Transform>(entity);
    transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
    
    using namespace OpenEye::Fluids::Templates;
    FireSmokeConfig config;
    config.position = transform.position;
    auto fluidSim = CreateFireSmoke(config);
    if (fluidSim) {
        auto& fluidComp = registry.emplace<OpenEye::ECS::FluidVolumeComponent>(entity);
        fluidComp.simulation = fluidSim;
        fluidComp.resolution = config.resolution;
        fluidComp.volumeSize = config.volumeSize;
        fluidComp.enabled = true;
    }
}

void FluidPanel::ApplyWaterPreset(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& transform = registry.get<OpenEye::ECS::Transform>(entity);
    transform.position = glm::vec3(0.0f, 0.5f, 0.0f);
    
    using namespace OpenEye::Fluids::Templates;
    WaterConfig config;
    config.position = transform.position;
    auto fluidSim = CreateWater(config);
    if (fluidSim) {
        auto& fluidComp = registry.emplace<OpenEye::ECS::FluidVolumeComponent>(entity);
        fluidComp.simulation = fluidSim;
        fluidComp.resolution = config.resolution;
        fluidComp.volumeSize = config.volumeSize;
        fluidComp.enabled = true;
    }
}

void FluidPanel::ApplyExplosionPreset(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& transform = registry.get<OpenEye::ECS::Transform>(entity);
    transform.position = glm::vec3(0.0f, 1.5f, 0.0f);
    
    using namespace OpenEye::Fluids::Templates;
    ExplosionConfig config;
    config.position = transform.position;
    auto fluidSim = CreateExplosion(config);
    if (fluidSim) {
        auto& fluidComp = registry.emplace<OpenEye::ECS::FluidVolumeComponent>(entity);
        fluidComp.simulation = fluidSim;
        fluidComp.resolution = config.resolution;
        fluidComp.volumeSize = config.volumeSize;
        fluidComp.enabled = true;
    }
}

} // namespace Editor
} // namespace OpenMeta
