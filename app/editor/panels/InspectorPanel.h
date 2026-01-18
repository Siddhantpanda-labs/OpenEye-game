#pragma once

#include "IEditorPanel.h"
#include "ecs/api/Scene.h"
#include <string>

namespace OpenMeta {
namespace Editor {

/**
 * @brief Inspector panel - edit components of selected entity
 */
class InspectorPanel : public IEditorPanel {
public:
    InspectorPanel(OpenEye::ECS::Scene* scene);
    
    void Render() override;
    const char* GetName() const override { return "Inspector"; }
    
private:
    void RenderTransform(OpenEye::ECS::Entity entity);
    void RenderName(OpenEye::ECS::Entity entity);
    void RenderCamera(OpenEye::ECS::Entity entity);
    void RenderLight(OpenEye::ECS::Entity entity);
    void RenderRenderable(OpenEye::ECS::Entity entity);
    void RenderAnimation(OpenEye::ECS::Entity entity);
    void RenderPhysics(OpenEye::ECS::Entity entity);
    void RenderAddComponentMenu(OpenEye::ECS::Entity entity);
    
    OpenEye::ECS::Scene* m_scene = nullptr;
};

} // namespace Editor
} // namespace OpenMeta

