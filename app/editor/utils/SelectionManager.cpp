#include "SelectionManager.h"

namespace OpenMeta {
namespace Editor {

SelectionManager& SelectionManager::GetInstance() {
    static SelectionManager instance;
    return instance;
}

void SelectionManager::SetSelectedEntity(OpenEye::ECS::Entity entity) {
    m_selectedEntity = entity;
    m_selectedEntities.clear();
    if (entity != OpenEye::ECS::Entity{}) {
        m_selectedEntities.push_back(entity);
    }
}

OpenEye::ECS::Entity SelectionManager::GetSelectedEntity() const {
    return m_selectedEntity;
}

bool SelectionManager::HasSelection() const {
    return m_selectedEntity != OpenEye::ECS::Entity{};
}

void SelectionManager::ClearSelection() {
    m_selectedEntity = OpenEye::ECS::Entity{};
    m_selectedEntities.clear();
}

void SelectionManager::AddToSelection(OpenEye::ECS::Entity entity) {
    if (std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity) == m_selectedEntities.end()) {
        m_selectedEntities.push_back(entity);
    }
    m_selectedEntity = entity; // Set as primary selection
}

bool SelectionManager::IsSelected(OpenEye::ECS::Entity entity) const {
    return std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity) != m_selectedEntities.end();
}

} // namespace Editor
} // namespace OpenMeta

