#include "HierarchyPanel.h"
#include "../utils/SelectionManager.h"
#include "ecs/api/All.h"
#include "imgui.h"
#include <vector>
#include <algorithm>

namespace OpenMeta {
namespace Editor {

HierarchyPanel::HierarchyPanel(OpenEye::ECS::Scene* scene) 
    : m_scene(scene) {
}

void HierarchyPanel::Render() {
    if (!m_isOpen || !m_scene) return;
    
    ImGui::Begin("Hierarchy", &m_isOpen);
    
    // Right-click context menu for empty space
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu")) {
        RenderAddEntityMenu();
        ImGui::EndPopup();
    }
    
    // Render all root entities (no parent)
    auto& registry = m_scene->GetRegistry();
    auto view = registry.view<OpenEye::ECS::Transform, OpenEye::ECS::Name>();
    
    std::vector<OpenEye::ECS::Entity> rootEntities;
    for (auto entity : view) {
        auto& transform = view.get<OpenEye::ECS::Transform>(entity);
        if (transform.parent == OpenEye::ECS::Entity{}) {
            rootEntities.push_back(entity);
        }
    }
    
    // Render root entities
    for (auto entity : rootEntities) {
        RenderEntityNode(entity);
    }
    
    // Handle drag-drop
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY")) {
            OpenEye::ECS::Entity droppedEntity = *(OpenEye::ECS::Entity*)payload->Data;
            
            // Remove from parent
            if (registry.all_of<OpenEye::ECS::Transform>(droppedEntity)) {
                auto& transform = registry.get<OpenEye::ECS::Transform>(droppedEntity);
                transform.parent = OpenEye::ECS::Entity{};
                transform.dirty = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    ImGui::End();
}

void HierarchyPanel::RenderEntityNode(OpenEye::ECS::Entity entity) {
    if (!m_scene) return;
    
    auto& registry = m_scene->GetRegistry();
    if (!registry.all_of<OpenEye::ECS::Name>(entity)) return;
    
    auto& name = registry.get<OpenEye::ECS::Name>(entity);
    auto& selectionManager = SelectionManager::GetInstance();
    
    // Build display name
    std::string displayName = name.name;
    
    // Add component icons
    std::string iconText = "";
    if (registry.all_of<OpenEye::ECS::CameraComponent>(entity)) iconText += "[C] ";
    if (registry.all_of<OpenEye::ECS::Light>(entity)) iconText += "[L] ";
    if (registry.all_of<OpenEye::ECS::Renderable>(entity)) iconText += "[M] ";
    if (registry.all_of<OpenEye::ECS::AnimationComponent>(entity)) iconText += "[A] ";
    
    displayName = iconText + displayName;
    
    // Check if entity has children
    bool hasChildren = false;
    auto view = registry.view<OpenEye::ECS::Transform>();
    for (auto child : view) {
        auto& childTransform = view.get<OpenEye::ECS::Transform>(child);
        if (childTransform.parent == entity) {
            hasChildren = true;
            break;
        }
    }
    
    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
    if (selectionManager.GetSelectedEntity() == entity) flags |= ImGuiTreeNodeFlags_Selected;
    
    // Render tree node
    bool nodeOpen = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", displayName.c_str());
    
    // Handle selection
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selectionManager.SetSelectedEntity(entity);
    }
    
    // Right-click context menu
    if (ImGui::BeginPopupContextItem()) {
        RenderContextMenu(entity);
        ImGui::EndPopup();
    }
    
    // Drag-drop source
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("ENTITY", &entity, sizeof(OpenEye::ECS::Entity));
        ImGui::Text("%s", name.name.c_str());
        ImGui::EndDragDropSource();
    }
    
    // Drag-drop target
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY")) {
            OpenEye::ECS::Entity droppedEntity = *(OpenEye::ECS::Entity*)payload->Data;
            
            // Prevent parenting to self or descendants
            bool isValid = true;
            if (droppedEntity == entity) isValid = false;
            
            // Check if entity is a descendant
            OpenEye::ECS::Entity current = entity;
            while (isValid && registry.all_of<OpenEye::ECS::Transform>(current)) {
                auto& transform = registry.get<OpenEye::ECS::Transform>(current);
                if (transform.parent == droppedEntity) {
                    isValid = false;
                    break;
                }
                current = transform.parent;
            }
            
            if (isValid) {
                if (registry.all_of<OpenEye::ECS::Transform>(droppedEntity)) {
                    auto& transform = registry.get<OpenEye::ECS::Transform>(droppedEntity);
                    transform.parent = entity;
                    transform.dirty = true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    // Render children
    if (nodeOpen) {
        if (hasChildren) {
            for (auto child : view) {
                auto& childTransform = view.get<OpenEye::ECS::Transform>(child);
                if (childTransform.parent == entity) {
                    RenderEntityNode(child);
                }
            }
        }
        ImGui::TreePop();
    }
}

void HierarchyPanel::RenderContextMenu(OpenEye::ECS::Entity entity) {
    auto& registry = m_scene->GetRegistry();
    
    if (ImGui::MenuItem("Delete")) {
        m_scene->DestroyEntity(entity);
        SelectionManager::GetInstance().ClearSelection();
    }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem("Add Child")) {
        auto child = m_scene->CreateEntity("Child");
        if (registry.all_of<OpenEye::ECS::Transform>(child) && registry.all_of<OpenEye::ECS::Transform>(entity)) {
            auto& childTransform = registry.get<OpenEye::ECS::Transform>(child);
            auto& parentTransform = registry.get<OpenEye::ECS::Transform>(entity);
            childTransform.parent = entity;
            childTransform.dirty = true;
        }
    }
    
    ImGui::Separator();
    RenderAddEntityMenu();
}

void HierarchyPanel::RenderAddEntityMenu() {
    if (ImGui::BeginMenu("Add Entity")) {
        if (ImGui::MenuItem("Empty Entity")) {
            m_scene->CreateEntity("Entity");
        }
        
        if (ImGui::MenuItem("Camera")) {
            auto entity = m_scene->CreateEntity("Camera");
            auto& registry = m_scene->GetRegistry();
            registry.emplace<OpenEye::ECS::CameraComponent>(entity);
            
            // Set as primary if no other camera exists
            bool hasPrimary = false;
            auto cameraView = registry.view<OpenEye::ECS::CameraComponent>();
            for (auto cam : cameraView) {
                if (cameraView.get<OpenEye::ECS::CameraComponent>(cam).isPrimary) {
                    hasPrimary = true;
                    break;
                }
            }
            if (!hasPrimary) {
                registry.get<OpenEye::ECS::CameraComponent>(entity).isPrimary = true;
            }
        }
        
        if (ImGui::MenuItem("Light")) {
            auto entity = m_scene->CreateEntity("Light");
            auto& registry = m_scene->GetRegistry();
            registry.emplace<OpenEye::ECS::Light>(entity);
        }
        
        if (ImGui::MenuItem("3D Object")) {
            m_scene->CreateEntity("3D Object");
        }
        
        ImGui::EndMenu();
    }
}

} // namespace Editor
} // namespace OpenMeta

