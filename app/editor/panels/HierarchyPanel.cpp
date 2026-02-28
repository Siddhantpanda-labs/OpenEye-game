#include "HierarchyPanel.h"
#include "EditorContext.h"
#include "PanelCommon.h"

#include "api/world/All.h"

#include <imgui.h>
#include <cstdio>

namespace openeye::editor::panels {

namespace {

using EntityId = ::eyelib::world::EntityId;

constexpr const char* kEntityPayload = "OpenEyeEntity";

/** True if \a entity is a descendant of \a potentialAncestor (i.e. potentialAncestor is in the parent chain of entity). */
bool IsDescendant(eyelib::api::world::World& world, EntityId entity, EntityId potentialAncestor) {
    EntityId current = world.GetParent(entity);
    while (current != ::eyelib::world::InvalidEntity) {
        if (current.index == potentialAncestor.index && current.generation == potentialAncestor.generation)
            return true;
        current = world.GetParent(current);
    }
    return false;
}

/** Scene display name from scene file path (e.g. "scenes/main.json" -> "main"). */
std::string GetSceneDisplayName(const std::string& scenePath) {
    if (scenePath.empty()) return "Scene";
    std::string name = scenePath;
    const size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const size_t dot = name.rfind('.');
    if (dot != std::string::npos && dot > 0) name = name.substr(0, dot);
    return name.empty() ? "Scene" : name;
}

std::string GetDisplayName(eyelib::api::world::World& world, EntityId id) {
    std::string name;
    if (world.GetEntityName(id, name)) {
        if (!name.empty()) return name;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Entity %u", id.index);
    return buf;
}

void ShowEntityNode(EditorContext* ctx, EntityId entity, int depth) {
    if (!ctx || !ctx->world.IsAlive(entity)) return;
    eyelib::api::world::World& world = ctx->world;

    std::string label = GetDisplayName(world, entity);
    const bool isSelected = (ctx->selectedEntity.index == entity.index && ctx->selectedEntity.generation == entity.generation);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
        | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;
    const std::vector<EntityId> children = world.GetChildren(entity);
    if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

    const bool open = ImGui::TreeNodeEx(
        (void*)(uintptr_t)(entity.index ^ (entity.generation << 16)),
        flags,
        "%s",
        label.c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        ctx->sceneSelected = false;
        ctx->selectedEntity = entity;
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Create Child")) {
            auto result = world.ApplyCommandSync(eyelib::api::world::CreateEntity("Entity"));
            if (result.success && result.createdEntity) {
                world.SetParent(*result.createdEntity, entity);
                ctx->selectedEntity = *result.createdEntity;
            }
        }
        if (ImGui::MenuItem("Rename...")) {
            ctx->selectedEntity = entity;
            ImGui::PushID("Hierarchy");
            ImGui::OpenPopup("Rename Entity");
            ImGui::PopID();
        }
        if (ImGui::MenuItem("Delete")) {
            world.DestroyEntity(entity);
            if (ctx->selectedEntity.index == entity.index && ctx->selectedEntity.generation == entity.generation) {
                ctx->selectedEntity = ::eyelib::world::InvalidEntity;
                ctx->sceneSelected = false;
            }
        }
        ImGui::EndPopup();
    }

    // Drag source: drag this entity to reparent it
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload(kEntityPayload, &entity, sizeof(EntityId));
        ImGui::Text("Reparent %s", label.c_str());
        ImGui::EndDragDropSource();
    }
    // Drop target: drop an entity here to make it a child of this entity
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityPayload)) {
            if (payload->DataSize == sizeof(EntityId)) {
                const EntityId dragged = *static_cast<const EntityId*>(payload->Data);
                if (dragged.index != entity.index || dragged.generation != entity.generation) {
                    if (world.IsAlive(dragged) && !IsDescendant(world, entity, dragged)) {
                        world.SetParent(dragged, entity);
                    } else if (world.IsAlive(dragged)) {
                        ImGui::SetTooltip("Cannot make a parent a child of its descendant");
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (open) {
        for (EntityId child : children) {
            ShowEntityNode(ctx, child, depth + 1);
        }
        ImGui::TreePop();
    }
}

void RunRenamePopup(EditorContext* ctx) {
    ImGui::PushID("Hierarchy");
    const bool open = ImGui::BeginPopup("Rename Entity", ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopID();
    if (!open) return;
    if (ctx->selectedEntity == ::eyelib::world::InvalidEntity || !ctx->world.IsAlive(ctx->selectedEntity)) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    static char renameBuffer[256] = "";
    if (ImGui::IsWindowAppearing()) {
        std::string name;
        if (ctx->world.GetEntityName(ctx->selectedEntity, name)) {
            std::snprintf(renameBuffer, sizeof(renameBuffer), "%s", name.c_str());
        } else {
            renameBuffer[0] = '\0';
        }
    }
    ImGui::SetNextItemWidth(200.0f);
    ImGui::PushID("HierarchyRename");
    ImGui::InputText("##name", renameBuffer, sizeof(renameBuffer));
    if (ImGui::Button("OK", ImVec2(80, 0))) {
        ctx->world.SetEntityName(ctx->selectedEntity, renameBuffer);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopID();
    ImGui::EndPopup();
}

} // namespace

void ShowHierarchyPanel(EditorContext* ctx) {
    if (ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_None)) {
        if (!ctx) {
            ImGui::TextUnformatted("(No context)");
            ImGui::End();
            return;
        }

        RunRenamePopup(ctx);

        eyelib::api::world::World& world = ctx->world;

        if (ImGui::Button("Create Entity")) {
            auto result = world.ApplyCommandSync(eyelib::api::world::CreateEntity("Entity"));
            if (result.success && result.createdEntity) {
                ctx->sceneSelected = false;
                ctx->selectedEntity = *result.createdEntity;
            }
        }

        ImGui::Separator();

        // Scene root (name from scene file); selecting it shows skybox in Inspector
        const std::string sceneName = GetSceneDisplayName(ctx->scenePath);
        const bool sceneIsSelected = ctx->sceneSelected;
        ImGuiTreeNodeFlags sceneFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
            | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
        if (sceneIsSelected) sceneFlags |= ImGuiTreeNodeFlags_Selected;
        const bool sceneOpen = ImGui::TreeNodeEx(
            (void*)(uintptr_t)0xFFFFFFFFu,
            sceneFlags,
            "%s",
            sceneName.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            ctx->sceneSelected = true;
            ctx->selectedEntity = ::eyelib::world::InvalidEntity;
        }
        // Drop target: drop an entity on scene root to make it a root (unparent)
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityPayload)) {
                if (payload->DataSize == sizeof(EntityId)) {
                    const EntityId dragged = *static_cast<const EntityId*>(payload->Data);
                    if (ctx->world.IsAlive(dragged))
                        ctx->world.ClearParent(dragged);
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (sceneOpen) {
            const std::vector<EntityId> all = world.GetEntities();
            for (EntityId entity : all) {
                if (!world.IsAlive(entity)) continue;
                if (world.GetParent(entity) != ::eyelib::world::InvalidEntity)
                    continue;  // only roots under scene
                ShowEntityNode(ctx, entity, 0);
            }
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

} // namespace openeye::editor::panels
