#include "ParticlePanel.h"
#include "ecs/api/All.h"
#include "ecs/Components.h"
#include "particles/ParticleEmitter.h"
#include "particles/templates/ParticleTemplates.h"
#include "imgui.h"
#include <glm/gtc/type_ptr.hpp>

namespace OpenMeta {
namespace Editor {

ParticlePanel::ParticlePanel(OpenEye::ECS::Scene* scene) : m_scene(scene) {}

void ParticlePanel::Render() {
    if (!m_isOpen || !m_scene) return;
    ImGui::Begin("Particles", &m_isOpen);
    
    auto& registry = m_scene->GetRegistry();
    auto emitterView = registry.view<OpenEye::ECS::ParticleEmitterComponent, OpenEye::ECS::Transform>();
    std::vector<OpenEye::ECS::Entity> emitters;
    for (auto entity : emitterView) emitters.push_back(entity);
    ImGui::Text("Particle Emitters (%zu)", emitters.size());
    if (ImGui::Button("Add Emitter")) {
        auto emitterEntity = m_scene->CreateEntity("ParticleEmitter");
        auto& transform = registry.get<OpenEye::ECS::Transform>(emitterEntity);
        transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        OpenEye::Particles::ParticleEmitterConfig config;
        config.position = transform.position;
        config.direction = glm::vec3(0.0f, 1.0f, 0.0f);
        config.particlesPerSecond = 50.0f;
        config.maxParticles = 500;
        auto emitter = std::make_shared<OpenEye::Particles::ParticleEmitter>(config);
        emitter->Start();
        registry.emplace<OpenEye::ECS::ParticleEmitterComponent>(emitterEntity, emitter);
    }
    ImGui::SameLine();
    // Use a unique popup name for this panel to avoid ID collisions with other panels
    if (ImGui::Button("Presets")) ImGui::OpenPopup("ParticlePresetPopup");
    if (ImGui::BeginPopup("ParticlePresetPopup")) {
        if (ImGui::MenuItem("Fire")) { auto e = m_scene->CreateEntity("FireEmitter"); ApplyFirePreset(e); }
        if (ImGui::MenuItem("Water")) { auto e = m_scene->CreateEntity("WaterEmitter"); ApplyWaterPreset(e); }
        if (ImGui::MenuItem("Explosion")) { auto e = m_scene->CreateEntity("ExplosionEmitter"); ApplyExplosionPreset(e); }
        ImGui::EndPopup();
    }
    ImGui::Separator();
    for (size_t i = 0; i < emitters.size(); i++) {
        auto entity = emitters[i];
        // Safety check: ensure entity still exists and has Name component
        if (!registry.valid(entity) || !registry.all_of<OpenEye::ECS::Name>(entity)) {
            continue;
        }
        auto& name = registry.get<OpenEye::ECS::Name>(entity);
        bool isSelected = (m_selectedEmitter == entity);
        if (ImGui::Selectable(name.name.c_str(), isSelected)) m_selectedEmitter = entity;
        ImGui::SameLine();
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton("X")) {
            if (m_selectedEmitter == entity) m_selectedEmitter = OpenEye::ECS::Entity{};
            m_scene->DestroyEntity(entity);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    // Safety check: validate selected entity exists and has required components before rendering
    if (m_selectedEmitter != OpenEye::ECS::Entity{} && 
        registry.valid(m_selectedEmitter) &&
        registry.all_of<OpenEye::ECS::ParticleEmitterComponent, OpenEye::ECS::Transform>(m_selectedEmitter)) {
        auto& emitterComp = registry.get<OpenEye::ECS::ParticleEmitterComponent>(m_selectedEmitter);
        RenderParticleEmitter(m_selectedEmitter, emitterComp);
    } else if (m_selectedEmitter != OpenEye::ECS::Entity{}) {
        // Selected entity no longer exists or is invalid, clear selection
        m_selectedEmitter = OpenEye::ECS::Entity{};
    }
    ImGui::End();
}

void ParticlePanel::RenderParticleEmitter(OpenEye::ECS::Entity entity, OpenEye::ECS::ParticleEmitterComponent& emitterComp) {
    if (!emitterComp.emitter) return;
    auto& registry = m_scene->GetRegistry();
    // Safety check: ensure entity still exists and has Transform component
    if (!registry.valid(entity) || !registry.all_of<OpenEye::ECS::Transform>(entity)) {
        return;
    }
    auto& transform = registry.get<OpenEye::ECS::Transform>(entity);
    auto& config = emitterComp.emitter->GetConfig();
    ImGui::Text("Emitter Properties");
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", glm::value_ptr(transform.position), 0.1f);
        config.position = transform.position;
        transform.dirty = true;
    }
    if (ImGui::CollapsingHeader("Emission", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Direction", glm::value_ptr(config.direction), 0.01f, -1.0f, 1.0f);
        config.direction = glm::normalize(config.direction);
        ImGui::DragFloat("Spread (degrees)", &config.spread, 1.0f, 0.0f, 180.0f);
        ImGui::DragFloat("Min Speed", &config.minSpeed, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Max Speed", &config.maxSpeed, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Particles/Second", &config.particlesPerSecond, 1.0f, 0.0f, 1000.0f);
        ImGui::DragInt("Max Particles", &config.maxParticles, 10, 0, 10000);
        ImGui::DragFloat("Spawn Radius", &config.spawnRadius, 0.01f, 0.0f, 5.0f);
    }
    if (ImGui::CollapsingHeader("Lifetime")) {
        ImGui::DragFloat("Min Lifetime", &config.minLifetime, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Max Lifetime", &config.maxLifetime, 0.1f, 0.0f, 10.0f);
    }
    if (ImGui::CollapsingHeader("Size")) {
        ImGui::DragFloat("Min Size", &config.minSize, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Max Size", &config.maxSize, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Size Over Lifetime", &config.sizeOverLifetime, 0.01f, -1.0f, 1.0f);
    }
    if (ImGui::CollapsingHeader("Color")) {
        ImGui::ColorEdit4("Start Color", glm::value_ptr(config.startColor));
        ImGui::ColorEdit4("End Color", glm::value_ptr(config.endColor));
    }
    if (ImGui::CollapsingHeader("Physics")) {
        ImGui::Checkbox("Use Gravity", &config.useGravity);
        if (config.useGravity) ImGui::DragFloat3("Gravity", glm::value_ptr(config.gravity), 0.1f, -20.0f, 20.0f);
        ImGui::DragFloat("Damping", &config.damping, 0.01f, 0.0f, 1.0f);
    }
    if (ImGui::CollapsingHeader("Control")) {
        bool isPlaying = emitterComp.emitter->IsPlaying();
        if (ImGui::Checkbox("Playing", &isPlaying)) {
            if (isPlaying) emitterComp.emitter->Start();
            else emitterComp.emitter->Stop();
        }
        if (ImGui::Button("Reset")) emitterComp.emitter->Reset();
        ImGui::SameLine();
        if (ImGui::Button("Burst (100)")) emitterComp.emitter->Emit(100);
        ImGui::Text("Active Particles: %zu", emitterComp.emitter->GetActiveParticleCount());
    }
    emitterComp.emitter->SetConfig(config);
}

void ParticlePanel::ApplyFirePreset(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& transform = registry.get<OpenEye::ECS::Transform>(entity);
    transform.position = glm::vec3(0.0f, 0.5f, 0.0f);
    
    using namespace OpenEye::Particles::Templates;
    FireConfig config;
    config.position = transform.position;
    auto emitter = CreateFire(config);
    registry.emplace<OpenEye::ECS::ParticleEmitterComponent>(entity, emitter);
}

void ParticlePanel::ApplyWaterPreset(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& transform = registry.get<OpenEye::ECS::Transform>(entity);
    transform.position = glm::vec3(0.0f, 2.0f, 0.0f);
    
    using namespace OpenEye::Particles::Templates;
    WaterConfig config;
    config.position = transform.position;
    auto emitter = CreateWater(config);
    registry.emplace<OpenEye::ECS::ParticleEmitterComponent>(entity, emitter);
}

void ParticlePanel::ApplyExplosionPreset(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    auto& transform = registry.get<OpenEye::ECS::Transform>(entity);
    transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
    
    using namespace OpenEye::Particles::Templates;
    ExplosionConfig config;
    config.position = transform.position;
    auto emitter = CreateExplosion(config);
    registry.emplace<OpenEye::ECS::ParticleEmitterComponent>(entity, emitter);
}

} // namespace Editor
} // namespace OpenMeta
