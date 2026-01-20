#pragma once

#include "IEditorPanel.h"
#include "ecs/api/Scene.h"

// Forward declarations
namespace OpenEye { namespace ECS {
    struct FluidVolumeComponent;
}}

namespace OpenMeta {
namespace Editor {

class FluidPanel : public IEditorPanel {
public:
    FluidPanel(OpenEye::ECS::Scene* scene);
    void Render() override;
    const char* GetName() const override { return "Fluids"; }
private:
    void RenderFluidVolume(OpenEye::ECS::Entity entity, OpenEye::ECS::FluidVolumeComponent& fluidComp);
    void ApplyFirePreset(OpenEye::ECS::Entity entity);
    void ApplyWaterPreset(OpenEye::ECS::Entity entity);
    void ApplyExplosionPreset(OpenEye::ECS::Entity entity);
    OpenEye::ECS::Scene* m_scene = nullptr;
    OpenEye::ECS::Entity m_selectedVolume = OpenEye::ECS::Entity{};
};

} // namespace Editor
} // namespace OpenMeta
