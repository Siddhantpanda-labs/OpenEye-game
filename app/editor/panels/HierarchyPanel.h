#pragma once

#include "IEditorPanel.h"
#include "ecs/api/Scene.h"
#include <string>

namespace OpenMeta {
namespace Editor {

/**
 * @brief Hierarchy panel - shows scene tree with drag-drop parent/child support
 */
class HierarchyPanel : public IEditorPanel {
public:
    HierarchyPanel(OpenEye::ECS::Scene* scene);
    
    void Render() override;
    const char* GetName() const override { return "Hierarchy"; }
    
private:
    void RenderEntityNode(OpenEye::ECS::Entity entity);
    void RenderContextMenu(OpenEye::ECS::Entity entity);
    void RenderAddEntityMenu();
    
    OpenEye::ECS::Scene* m_scene = nullptr;
    OpenEye::ECS::Entity m_draggedEntity = OpenEye::ECS::Entity{};
    OpenEye::ECS::Entity m_dragTargetEntity = OpenEye::ECS::Entity{};
};

} // namespace Editor
} // namespace OpenMeta

