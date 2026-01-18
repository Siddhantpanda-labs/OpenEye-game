#pragma once

#include "ecs/api/Scene.h"
#include <optional>
#include <vector>

namespace OpenMeta {
namespace Editor {

/**
 * @brief Manages selection state in the editor
 */
class SelectionManager {
public:
    static SelectionManager& GetInstance();
    
    void SetSelectedEntity(OpenEye::ECS::Entity entity);
    OpenEye::ECS::Entity GetSelectedEntity() const;
    bool HasSelection() const;
    void ClearSelection();
    
    // For multi-selection in the future
    void AddToSelection(OpenEye::ECS::Entity entity);
    const std::vector<OpenEye::ECS::Entity>& GetSelection() const { return m_selectedEntities; }
    bool IsSelected(OpenEye::ECS::Entity entity) const;

private:
    SelectionManager() = default;
    OpenEye::ECS::Entity m_selectedEntity = OpenEye::ECS::Entity{};
    std::vector<OpenEye::ECS::Entity> m_selectedEntities;
};

} // namespace Editor
} // namespace OpenMeta

