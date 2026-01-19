#include "AnimationPanel.h"
#include "ecs/api/All.h"
#include "animation/AnimationSystem.h"
#include "imgui.h"
#include <algorithm>

namespace OpenMeta {
namespace Editor {

AnimationPanel::AnimationPanel(OpenEye::ECS::Scene* scene)
    : m_scene(scene) {
}

void AnimationPanel::Render() {
    if (!m_isOpen || !m_scene) return;
    
    ImGui::Begin("Animation", &m_isOpen);
    
    auto& registry = m_scene->GetRegistry();
    
    // Collect all entities with animations
    auto animView = registry.view<OpenEye::ECS::AnimationComponent>();
    std::vector<OpenEye::ECS::Entity> animatedEntities;
    animatedEntities.reserve(animView.size());
    for (auto entity : animView) {
        animatedEntities.push_back(entity);
    }
    
    if (animatedEntities.empty()) {
        ImGui::Text("No animated entities in scene");
        ImGui::End();
        return;
    }
    
    ImGui::Text("Animated Entities (%zu)", animatedEntities.size());
    ImGui::Separator();
    
    // Entity list
    for (size_t i = 0; i < animatedEntities.size(); ++i) {
        auto entity = animatedEntities[i];
        std::string entityName = "Entity " + std::to_string(static_cast<uint32_t>(entity));
        if (registry.all_of<OpenEye::ECS::Name>(entity)) {
            auto& name = registry.get<OpenEye::ECS::Name>(entity);
            entityName = name.name;
        }
        
        bool isSelected = (m_selectedEntityIndex == static_cast<int>(i));
        if (ImGui::Selectable((entityName + "##" + std::to_string(i)).c_str(), isSelected)) {
            m_selectedEntityIndex = static_cast<int>(i);
        }
    }
    
    ImGui::Separator();
    
    // Animation controls for selected entity
    if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(animatedEntities.size())) {
        auto entity = animatedEntities[m_selectedEntityIndex];
        auto& anim = registry.get<OpenEye::ECS::AnimationComponent>(entity);
        
        RenderAnimationControls(entity, anim);
    }
    
    ImGui::End();
}

void AnimationPanel::RenderAnimationControls(OpenEye::ECS::Entity entity, OpenEye::ECS::AnimationComponent& anim) {
    auto& registry = m_scene->GetRegistry();
    
    ImGui::Text("Animation Controls");
    
    // Clip selection
    if (anim.clips.empty()) {
        ImGui::Text("No animation clips available");
        return;
    }
    
    // Build clip names list
    std::vector<const char*> clipNames;
    for (size_t i = 0; i < anim.clips.size(); ++i) {
        // We can't easily get the name without casting, so use index
        clipNames.push_back(("Clip " + std::to_string(i)).c_str());
    }
    
    int currentClip = anim.currentClipIndex;
    if (currentClip < 0) currentClip = 0;
    if (currentClip >= static_cast<int>(anim.clips.size())) currentClip = 0;
    
    if (ImGui::Combo("Animation Clip", &currentClip, clipNames.data(), static_cast<int>(clipNames.size()))) {
            if (currentClip >= 0 && currentClip < static_cast<int>(anim.clips.size())) {
                using namespace OpenEye::Animation;
                AnimationSystem::PlayAnimation(registry, entity, currentClip, 0.2f);
            }
    }
    
    // Playback controls
    ImGui::Separator();
    
    using namespace OpenEye::Animation;
    
    if (ImGui::Button(anim.isPlaying ? "Pause" : "Play")) {
        if (anim.isPlaying) {
            AnimationSystem::StopAnimation(registry, entity);
        } else {
            if (anim.currentClipIndex >= 0 && anim.currentClipIndex < static_cast<int>(anim.clips.size())) {
                AnimationSystem::PlayAnimation(registry, entity, anim.currentClipIndex, 0.0f);
            }
        }
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Stop")) {
        AnimationSystem::StopAnimation(registry, entity);
        anim.currentTime = 0.0f;
    }
    
    // Playback speed
    ImGui::DragFloat("Speed", &anim.playbackSpeed, 0.1f, 0.0f, 5.0f);
    
    // Loop toggle
    ImGui::Checkbox("Loop", &anim.loop);
    
    // Time scrubber
    float maxTime = 1.0f;
    if (anim.currentClipIndex >= 0 && anim.currentClipIndex < static_cast<int>(anim.clips.size())) {
        // We'd need to cast to get duration, but for now use a reasonable default
        maxTime = 10.0f; // Default max time
    }
    
    ImGui::DragFloat("Time", &anim.currentTime, 0.01f, 0.0f, maxTime);
    
    // Current clip info
    if (anim.currentClipIndex >= 0) {
        ImGui::Text("Current Clip: %d", anim.currentClipIndex);
        ImGui::Text("Playing: %s", anim.isPlaying ? "Yes" : "No");
    }
}

} // namespace Editor
} // namespace OpenMeta
