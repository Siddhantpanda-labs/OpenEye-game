#include "HierarchyPanel.h"
#include "EditorContext.h"
#include "PanelCommon.h"

#include "api/world/All.h"

#include <imgui.h>
#include <cstdio>

namespace openeye::editor::panels {

namespace {

using EntityId = ::eyelib::world::EntityId;

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
            }
        }
        ImGui::EndPopup();
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
                ctx->selectedEntity = *result.createdEntity;
            }
        }

        ImGui::Separator();

        const std::vector<EntityId> all = world.GetEntities();
        for (EntityId entity : all) {
            if (!world.IsAlive(entity)) continue;
            if (world.GetParent(entity) != ::eyelib::world::InvalidEntity)
                continue;  // only roots at top level
            ShowEntityNode(ctx, entity, 0);
        }
    }
    ImGui::End();
}

} // namespace openeye::editor::panels
