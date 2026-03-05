#include "HierarchyPanel.h"
#include "EditorContext.h"
#include "PanelCommon.h"

#include "api/geometry/BuiltinMeshIds.h"
#include "api/world/All.h"
#include "api/world/Commands.h"

#include <imgui.h>
#include <cstdio>
#include <unordered_set>

namespace openeye::editor::panels {

namespace {

using EntityId = ::eyelib::world::EntityId;

constexpr const char* kEntityPayload = "OpenEyeEntity";

/** Primitive mesh choice for Add Entity (mesh primitives only; no None). */
enum class AddEntityMeshChoice : int {
    Cube,
    Sphere,
    Plane,
    Cylinder,
    Cone,
    Torus,
};

const char* AddEntityMeshChoiceBuiltinId(AddEntityMeshChoice choice) {
    switch (choice) {
        case AddEntityMeshChoice::Cube:     return eyelib::api::geometry::BuiltinMesh::Cube;
        case AddEntityMeshChoice::Sphere:   return eyelib::api::geometry::BuiltinMesh::Sphere;
        case AddEntityMeshChoice::Plane:    return eyelib::api::geometry::BuiltinMesh::Plane;
        case AddEntityMeshChoice::Cylinder: return eyelib::api::geometry::BuiltinMesh::Cylinder;
        case AddEntityMeshChoice::Cone:    return eyelib::api::geometry::BuiltinMesh::Cone;
        case AddEntityMeshChoice::Torus:   return eyelib::api::geometry::BuiltinMesh::Torus;
    }
    return "";
}

const char* AddEntityMeshChoiceDefaultName(AddEntityMeshChoice choice) {
    switch (choice) {
        case AddEntityMeshChoice::Cube:     return "Cube";
        case AddEntityMeshChoice::Sphere:   return "Sphere";
        case AddEntityMeshChoice::Plane:    return "Plane";
        case AddEntityMeshChoice::Cylinder: return "Cylinder";
        case AddEntityMeshChoice::Cone:    return "Cone";
        case AddEntityMeshChoice::Torus:   return "Torus";
    }
    return "Entity";
}

std::string GetUniqueEntityName(eyelib::api::world::World& world, const std::string& baseName) {
    std::unordered_set<std::string> existing;
    for (EntityId id : world.GetEntities()) {
        if (!world.IsAlive(id)) continue;
        std::string n;
        if (world.GetEntityName(id, n) && !n.empty())
            existing.insert(n);
    }
    if (existing.find(baseName) == existing.end())
        return baseName;
    for (int i = 1; i < 10000; ++i) {
        std::string candidate = baseName + std::to_string(i);
        if (existing.find(candidate) == existing.end())
            return candidate;
    }
    return baseName + "_";
}

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

/** When non-null, the next entity created from Add Entity popup will be parented to this (e.g. from "Add Child"). */
static EntityId s_pendingAddParent = ::eyelib::world::InvalidEntity;
/** Set when "Add Child" is clicked; open "Add Entity" popup next frame from Hierarchy window (not from context menu). */
static bool s_openAddEntityPopupNextFrame = false;

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
        if (ImGui::MenuItem("Add Child")) {
            s_pendingAddParent = entity;
            s_openAddEntityPopupNextFrame = true;
        }
        if (ImGui::MenuItem("Remove")) {
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

/** Create entity with primitive mesh (Renderable + optional Collider), select it, close popups. Optionally parent to s_pendingAddParent. */
static void AddPrimitiveEntity(EditorContext* ctx, AddEntityMeshChoice choice) {
    if (!ctx) return;
    eyelib::api::world::World& world = ctx->world;
    const std::string name = GetUniqueEntityName(world, AddEntityMeshChoiceDefaultName(choice));
    auto result = world.ApplyCommandSync(eyelib::api::world::CreateEntity(name));
    if (!result.success || !result.createdEntity) return;
    EntityId newEntity = *result.createdEntity;
    if (s_pendingAddParent != ::eyelib::world::InvalidEntity && world.IsAlive(s_pendingAddParent)) {
        world.SetParent(newEntity, s_pendingAddParent);
        s_pendingAddParent = ::eyelib::world::InvalidEntity;
    }
    ctx->sceneSelected = false;
    ctx->selectedEntity = newEntity;

    const char* meshId = AddEntityMeshChoiceBuiltinId(choice);
    eyelib::world::components::Renderable renderable{};
    renderable.meshAssetId = meshId;
    renderable.visible = true;
    world.ApplyCommandSync(eyelib::api::world::AddComponent<eyelib::world::components::Renderable>(newEntity, renderable));

    using CT = eyelib::world::components::Collider::Type;
    bool addCollider = false;
    eyelib::world::components::Collider collider{};
    switch (choice) {
        case AddEntityMeshChoice::Cube:
            collider.type = CT::Box;
            collider.size = eyelib::math::Vec3(1.0f, 1.0f, 1.0f);
            addCollider = true;
            break;
        case AddEntityMeshChoice::Plane:
            collider.type = CT::Box;
            collider.size = eyelib::math::Vec3(10.0f, 0.01f, 10.0f);
            addCollider = true;
            break;
        case AddEntityMeshChoice::Sphere:
            collider.type = CT::Sphere;
            collider.radius = 0.5f;
            addCollider = true;
            break;
        case AddEntityMeshChoice::Cylinder:
        case AddEntityMeshChoice::Cone:
            collider.type = CT::Capsule;
            collider.radius = 0.5f;
            collider.height = 1.0f;
            addCollider = true;
            break;
        default:
            break;
    }
    if (addCollider) {
        world.ApplyCommandSync(eyelib::api::world::AddComponent<eyelib::world::components::Collider>(newEntity, collider));
    }
}

/** Create entity with Light component, select it. Optionally parent to s_pendingAddParent. */
static void AddLightEntity(EditorContext* ctx, eyelib::world::components::Light::Type lightType, const char* baseName) {
    if (!ctx) return;
    eyelib::api::world::World& world = ctx->world;
    const std::string name = GetUniqueEntityName(world, baseName);
    auto result = world.ApplyCommandSync(eyelib::api::world::CreateEntity(name));
    if (!result.success || !result.createdEntity) return;
    EntityId newEntity = *result.createdEntity;
    if (s_pendingAddParent != ::eyelib::world::InvalidEntity && world.IsAlive(s_pendingAddParent)) {
        world.SetParent(newEntity, s_pendingAddParent);
        s_pendingAddParent = ::eyelib::world::InvalidEntity;
    }
    ctx->sceneSelected = false;
    ctx->selectedEntity = newEntity;
    eyelib::world::components::Light light{};
    light.type = lightType;
    world.ApplyCommandSync(eyelib::api::world::AddComponent<eyelib::world::components::Light>(newEntity, light));
}

/** Create entity with Camera component, select it. Optionally parent to s_pendingAddParent. */
static void AddCameraEntity(EditorContext* ctx) {
    if (!ctx) return;
    eyelib::api::world::World& world = ctx->world;
    const std::string name = GetUniqueEntityName(world, "Camera");
    auto result = world.ApplyCommandSync(eyelib::api::world::CreateEntity(name));
    if (!result.success || !result.createdEntity) return;
    EntityId newEntity = *result.createdEntity;
    if (s_pendingAddParent != ::eyelib::world::InvalidEntity && world.IsAlive(s_pendingAddParent)) {
        world.SetParent(newEntity, s_pendingAddParent);
        s_pendingAddParent = ::eyelib::world::InvalidEntity;
    }
    ctx->sceneSelected = false;
    ctx->selectedEntity = newEntity;
    eyelib::world::components::Camera camera{};
    world.ApplyCommandSync(eyelib::api::world::AddComponent<eyelib::world::components::Camera>(newEntity, camera));
}

/** Create empty entity (Transform only, from default), select it. Optionally parent to s_pendingAddParent. */
static void AddEmptyEntity(EditorContext* ctx) {
    if (!ctx) return;
    eyelib::api::world::World& world = ctx->world;
    const std::string name = GetUniqueEntityName(world, "Entity");
    auto result = world.ApplyCommandSync(eyelib::api::world::CreateEntity(name));
    if (!result.success || !result.createdEntity) return;
    EntityId newEntity = *result.createdEntity;
    if (s_pendingAddParent != ::eyelib::world::InvalidEntity && world.IsAlive(s_pendingAddParent)) {
        world.SetParent(newEntity, s_pendingAddParent);
        s_pendingAddParent = ::eyelib::world::InvalidEntity;
    }
    ctx->sceneSelected = false;
    ctx->selectedEntity = newEntity;
}

void RunAddEntityPopup(EditorContext* ctx) {
    if (!ctx) return;

    if (!ImGui::BeginPopup("Add Entity", ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    if (ImGui::Selectable("Empty")) {
        AddEmptyEntity(ctx);
        ImGui::CloseCurrentPopup();
    }

    // Mesh section: label + ">" opens primitives sub-popup
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Mesh");
    ImGui::SameLine(80.0f);
    if (ImGui::Button(">")) {
        ImGui::OpenPopup("Primitive Mesh");
    }
    if (ImGui::BeginPopup("Primitive Mesh")) {
        auto addPrimitive = [&](AddEntityMeshChoice choice, const char* label) {
            if (ImGui::Selectable(label)) {
                AddPrimitiveEntity(ctx, choice);
                ImGui::CloseCurrentPopup();  // close Primitive Mesh
                ImGui::CloseCurrentPopup();  // close Add Entity
            }
        };
        addPrimitive(AddEntityMeshChoice::Cube, "Cube");
        addPrimitive(AddEntityMeshChoice::Sphere, "Sphere");
        addPrimitive(AddEntityMeshChoice::Plane, "Plane");
        addPrimitive(AddEntityMeshChoice::Cylinder, "Cylinder");
        addPrimitive(AddEntityMeshChoice::Cone, "Cone");
        addPrimitive(AddEntityMeshChoice::Torus, "Torus");
        ImGui::EndPopup();
    }

    // Light section
    using LightType = eyelib::world::components::Light::Type;
    if (ImGui::Selectable("Directional Light")) {
        AddLightEntity(ctx, LightType::Directional, "Directional Light");
        ImGui::CloseCurrentPopup();
    }
    if (ImGui::Selectable("Point Light")) {
        AddLightEntity(ctx, LightType::Point, "Point Light");
        ImGui::CloseCurrentPopup();
    }
    if (ImGui::Selectable("Spot Light")) {
        AddLightEntity(ctx, LightType::Spot, "Spot Light");
        ImGui::CloseCurrentPopup();
    }

    // Camera
    if (ImGui::Selectable("Camera")) {
        AddCameraEntity(ctx);
        ImGui::CloseCurrentPopup();
    }

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

        eyelib::api::world::World& world = ctx->world;

        ImGui::PushID("AddEntity");
        if (ImGui::Button("Add Entity")) {
            s_pendingAddParent = ::eyelib::world::InvalidEntity;
            ImGui::OpenPopup("Add Entity");
        }
        if (s_openAddEntityPopupNextFrame) {
            s_openAddEntityPopupNextFrame = false;
            ImGui::OpenPopup("Add Entity");
        }
        RunAddEntityPopup(ctx);
        ImGui::PopID();

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
