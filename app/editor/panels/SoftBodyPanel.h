#pragma once

#include "IEditorPanel.h"
#include "ecs/api/Scene.h"
#include "ecs/api/Components.h"
#include <memory>

namespace OpenMeta {
namespace Editor {

class SoftBodyPanel : public IEditorPanel {
public:
    SoftBodyPanel(OpenEye::ECS::Scene* scene);
    
    void Render() override;
    const char* GetName() const override { return "Soft Bodies"; }

private:
    OpenEye::ECS::Scene* m_scene = nullptr;
    
    void RenderSoftBody(OpenEye::ECS::Entity entity, OpenEye::ECS::SoftBodyComponent& softBodyComp);
    void ApplyClothPreset(OpenEye::ECS::Entity entity, const char* presetName);
};

} // namespace Editor
} // namespace OpenMeta
