#pragma once

#include "IEditorPanel.h"
#include "ecs/api/Scene.h"

// Forward declarations
namespace OpenEye { namespace ECS {
    struct ParticleEmitterComponent;
}}

namespace OpenMeta {
namespace Editor {

class ParticlePanel : public IEditorPanel {
public:
    ParticlePanel(OpenEye::ECS::Scene* scene);
    void Render() override;
    const char* GetName() const override { return "Particles"; }
private:
    void RenderParticleEmitter(OpenEye::ECS::Entity entity, OpenEye::ECS::ParticleEmitterComponent& emitterComp);
    void ApplyFirePreset(OpenEye::ECS::Entity entity);
    void ApplyWaterPreset(OpenEye::ECS::Entity entity);
    void ApplyExplosionPreset(OpenEye::ECS::Entity entity);
    OpenEye::ECS::Scene* m_scene = nullptr;
    OpenEye::ECS::Entity m_selectedEmitter = OpenEye::ECS::Entity{};
};

} // namespace Editor
} // namespace OpenMeta
