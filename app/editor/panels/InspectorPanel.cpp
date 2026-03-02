#include "InspectorPanel.h"
#include "EditorContext.h"
#include "PanelCommon.h"
#include "ui/PhysicsColliderIcons.h"

#include "api/world/All.h"
#include "api/world/Commands.h"

#include "eyelib/world/components/Collider.h"
#include "eyelib/world/components/RigidBody.h"

#include <imgui.h>
#include <json.hpp>
#include <string>
#include <cstdio>
#include <cstring>

namespace openeye::editor::panels {

namespace {

using EntityId = ::eyelib::world::EntityId;

void ShowNameComponent(eyelib::api::world::World& world, EntityId entity) {
    ImGui::PushID("InspectorName");
    ImGui::PushID(static_cast<int>(entity.index));
    ImGui::PushID(static_cast<int>(entity.generation));
    std::string jsonStr;
    if (!world.GetComponentJson(entity, "Name", jsonStr)) {
        ImGui::PopID();
        ImGui::PopID();
        ImGui::PopID();
        return;
    }
    nlohmann::json j = nlohmann::json::parse(jsonStr, nullptr, false);
    if (j.is_discarded() || !j.is_string()) {
        ImGui::PopID();
        ImGui::PopID();
        ImGui::PopID();
        return;
    }
    std::string value = j.get<std::string>();
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%.255s", value.c_str());
    if (ImGui::InputText("Name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        nlohmann::json newJ = std::string(buf);
        world.SetComponentJson(entity, "Name", newJ.dump());
        world.SetEntityName(entity, buf);
    }
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
}

void ShowColliderComponent(eyelib::api::world::World& world, EntityId entity) {
    ::eyelib::world::components::Collider c;
    if (!world.GetCollider(entity, c)) return;
    ImGui::PushID("InspectorCollider");
    ImGui::PushID(static_cast<int>(entity.index));
    ImGui::PushID(static_cast<int>(entity.generation));
    int typeIdx = static_cast<int>(c.type);
    const char* types[] = { "Box", "Sphere", "Capsule" };
    if (ImGui::Combo("Type", &typeIdx, types, 3)) {
        c.type = static_cast<::eyelib::world::components::Collider::Type>(typeIdx);
        world.SetCollider(entity, c);
    }
    float cen[3] = { c.center.x, c.center.y, c.center.z };
    if (ImGui::DragFloat3("Center", cen, 0.01f)) {
        c.center.x = cen[0]; c.center.y = cen[1]; c.center.z = cen[2];
        world.SetCollider(entity, c);
    }
    if (c.type == ::eyelib::world::components::Collider::Type::Box) {
        float sz[3] = { c.size.x, c.size.y, c.size.z };
        if (ImGui::DragFloat3("Size", sz, 0.01f, 0.001f, 1000.f)) {
            c.size.x = sz[0]; c.size.y = sz[1]; c.size.z = sz[2];
            world.SetCollider(entity, c);
        }
    } else {
        if (ImGui::DragFloat("Radius", &c.radius, 0.01f, 0.001f, 1000.f))
            world.SetCollider(entity, c);
        if (c.type == ::eyelib::world::components::Collider::Type::Capsule) {
            if (ImGui::DragFloat("Height", &c.height, 0.01f, 0.001f, 1000.f))
                world.SetCollider(entity, c);
        }
    }
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
}

void ShowRigidBodyComponent(eyelib::api::world::World& world, EntityId entity) {
    ::eyelib::world::components::RigidBody rb;
    if (!world.GetRigidBody(entity, rb)) return;
    ImGui::PushID("InspectorRigidBody");
    ImGui::PushID(static_cast<int>(entity.index));
    ImGui::PushID(static_cast<int>(entity.generation));
    if (ImGui::DragFloat("Mass", &rb.mass, 0.01f, 0.0f, 10000.0f, "%.3f")) {
        if (rb.mass < 0.0f) rb.mass = 0.0f;
        world.SetRigidBody(entity, rb);
    }
    if (ImGui::Checkbox("Use Gravity", &rb.useGravity))
        world.SetRigidBody(entity, rb);
    if (ImGui::Checkbox("Is Kinematic", &rb.isKinematic))
        world.SetRigidBody(entity, rb);
    if (ImGui::Checkbox("Sync Transform", &rb.syncTransform))
        world.SetRigidBody(entity, rb);
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
}

void ShowTransformComponent(eyelib::api::world::World& world, EntityId entity) {
    std::string jsonStr;
    if (!world.GetComponentJson(entity, "Transform", jsonStr)) return;
    nlohmann::json j = nlohmann::json::parse(jsonStr, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return;

    auto getVec3 = [&j](const char* key, float* v, int n) {
        if (!j.contains(key) || !j[key].is_array() || j[key].size() < (size_t)n) return;
        for (int i = 0; i < n; ++i) v[i] = j[key][i].get<float>();
    };
    auto setVec3 = [](nlohmann::json& out, const char* key, const float* v, int n) {
        out[key] = nlohmann::json::array();
        for (int i = 0; i < n; ++i) out[key].push_back(v[i]);
    };

    float pos[3], rot[4], scale[3];
    getVec3("position", pos, 3);
    getVec3("rotation", rot, 4);
    getVec3("scale", scale, 3);

    bool changed = false;
    if (ImGui::DragFloat3("Position", pos, 0.01f)) changed = true;
    if (ImGui::DragFloat4("Rotation (w,x,y,z)", rot, 0.01f)) changed = true;
    if (ImGui::DragFloat3("Scale", scale, 0.01f)) changed = true;

    if (changed) {
        nlohmann::json newJ;
        setVec3(newJ, "position", pos, 3);
        setVec3(newJ, "rotation", rot, 4);
        setVec3(newJ, "scale", scale, 3);
        world.SetComponentJson(entity, "Transform", newJ.dump());
    }
}

void ShowGenericComponent(eyelib::api::world::World& world, EntityId entity,
                          const std::string& componentName, const std::string& jsonStr) {
    ImGui::PushID(static_cast<int>(entity.index));
    ImGui::PushID(static_cast<int>(entity.generation));
    ImGui::PushID(componentName.c_str());
    if (!ImGui::CollapsingHeader(componentName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopID();
        ImGui::PopID();
        ImGui::PopID();
        return;
    }
    static EntityId s_entity = ::eyelib::world::InvalidEntity;
    static std::string s_component;
    static std::string s_editBuffer;
    if (s_entity.index != entity.index || s_entity.generation != entity.generation || s_component != componentName) {
        s_entity = entity;
        s_component = componentName;
        s_editBuffer = jsonStr;
    }
    const size_t bufSize = 4096;
    if (s_editBuffer.size() < bufSize) s_editBuffer.resize(bufSize, '\0');
    char idBuf[96];
    std::snprintf(idBuf, sizeof(idBuf), "##%u_%u_%s", entity.index, entity.generation, componentName.c_str());
    ImGui::InputTextMultiline(idBuf, s_editBuffer.data(), bufSize, ImVec2(-1, 80), ImGuiInputTextFlags_AllowTabInput);
    s_editBuffer.resize(strlen(s_editBuffer.data()));
    if (ImGui::Button("Apply")) {
        if (!s_editBuffer.empty()) {
            nlohmann::json j = nlohmann::json::parse(s_editBuffer, nullptr, false);
            if (!j.is_discarded())
                world.SetComponentJson(entity, componentName, s_editBuffer);
        }
    }
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
}

void ShowComponent(EditorContext* ctx, const std::string& componentName) {
    eyelib::api::world::World& world = ctx->world;
    EntityId entity = ctx->selectedEntity;

    std::string jsonStr;
    if (!world.GetComponentJson(entity, componentName, jsonStr)) return;

    if (componentName == "Name") {
        if (ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen))
            ShowNameComponent(world, entity);
        return;
    }
    if (componentName == "Transform") {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            ShowTransformComponent(world, entity);
        return;
    }
    if (componentName == "Collider") {
        if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen))
            ShowColliderComponent(world, entity);
        return;
    }
    if (componentName == "RigidBody") {
        if (ImGui::CollapsingHeader("RigidBody", ImGuiTreeNodeFlags_DefaultOpen))
            ShowRigidBodyComponent(world, entity);
        return;
    }

    ShowGenericComponent(world, entity, componentName, jsonStr);
}

constexpr float kColliderIconSize = 18.f;

void ShowAddComponentPopup(EditorContext* ctx) {
    if (!ctx || ctx->selectedEntity == ::eyelib::world::InvalidEntity || !ctx->world.IsAlive(ctx->selectedEntity))
        return;
    if (!ImGui::BeginPopup("AddComponent", ImGuiWindowFlags_AlwaysAutoResize))
        return;

    eyelib::api::world::World& world = ctx->world;
    EntityId entity = ctx->selectedEntity;
    using Collider = ::eyelib::world::components::Collider;
    using RigidBody = ::eyelib::world::components::RigidBody;

    static PhysicsColliderIcons s_icons;
    const bool iconsOk = s_icons.EnsureLoaded();
    const float iconSize = kColliderIconSize;

    ImGui::PushID(static_cast<int>(entity.index));
    ImGui::PushID(static_cast<int>(entity.generation));
    if (ImGui::TreeNodeEx("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto addCollider = [&](Collider::Type type, const char* label, ColliderIconShape iconShape) {
            ImGui::PushID(static_cast<int>(type));
            if (iconsOk) {
                uint64_t texId = s_icons.GetTexture(iconShape);
                if (texId)
                    ImGui::Image(static_cast<ImTextureID>(texId), ImVec2(iconSize, iconSize), ImVec2(0, 1), ImVec2(1, 0));
                ImGui::SameLine();
            }
            if (ImGui::Selectable(label)) {
                Collider c;
                c.type = type;
                c.size = eyelib::math::Vec3(1.f, 1.f, 1.f);
                c.radius = 0.5f;
                c.height = 1.f;
                world.ApplyCommandSync(eyelib::api::world::AddComponent<Collider>(entity, c));
            }
            ImGui::PopID();
        };
        addCollider(Collider::Type::Box, "Box", ColliderIconShape::Box);
        addCollider(Collider::Type::Sphere, "Sphere", ColliderIconShape::Sphere);
        addCollider(Collider::Type::Capsule, "Capsule", ColliderIconShape::Capsule);
        ImGui::PushID(-1);
        if (iconsOk && s_icons.GetTexture(ColliderIconShape::Mesh))
            ImGui::Image(static_cast<ImTextureID>(s_icons.GetTexture(ColliderIconShape::Mesh)), ImVec2(iconSize, iconSize), ImVec2(0, 1), ImVec2(1, 0));
        ImGui::SameLine();
        ImGui::Selectable("Convex Hull (disabled)", false);
        ImGui::Selectable("Mesh (disabled)", false);
        ImGui::PopID();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("RigidBody", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("RigidBody");
        if (iconsOk) {
            uint64_t texId = s_icons.GetRigidBodyTexture();
            if (texId)
                ImGui::Image(static_cast<ImTextureID>(texId), ImVec2(iconSize, iconSize), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
        }
        if (ImGui::Selectable("Rigidbody")) {
            RigidBody rb;
            world.ApplyCommandSync(eyelib::api::world::AddComponent<RigidBody>(entity, rb));
        }
        ImGui::PopID();
        ImGui::TreePop();
    }
    ImGui::PopID();
    ImGui::PopID();

    ImGui::EndPopup();
}

} // namespace

void ShowInspectorPanel(EditorContext* ctx) {
    if (ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_None)) {
        if (!ctx) {
            ImGui::TextUnformatted("(No context)");
        } else if (ctx->sceneSelected) {
            ImGui::TextUnformatted("Scene");
            ImGui::Separator();
            char skyboxBuf[256];
            std::string skyboxId = ctx->world.GetSkyboxAssetId();
            std::snprintf(skyboxBuf, sizeof(skyboxBuf), "%.255s", skyboxId.c_str());
            if (ImGui::InputText("Skybox (e.g. Skybox/Citadella)", skyboxBuf, sizeof(skyboxBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                ctx->world.SetSkyboxAssetId(skyboxBuf[0] ? std::string(skyboxBuf) : std::string());
            }
        } else if (ctx->selectedEntity == ::eyelib::world::InvalidEntity || !ctx->world.IsAlive(ctx->selectedEntity)) {
            ImGui::TextUnformatted("Select the scene or an entity in Hierarchy.");
        } else {
            ctx->sceneSelected = false;  // ensure entity selection takes precedence if both were set
            // Top name box: click to edit, click away or Enter to apply rename
            {
                static char nameBuffer[256] = "";
                static EntityId s_lastNameEntity = ::eyelib::world::InvalidEntity;
                EntityId e = ctx->selectedEntity;
                if (e.index != s_lastNameEntity.index || e.generation != s_lastNameEntity.generation) {
                    s_lastNameEntity = e;
                    std::string name;
                    if (ctx->world.GetEntityName(e, name))
                        std::snprintf(nameBuffer, sizeof(nameBuffer), "%.255s", name.c_str());
                    else
                        std::snprintf(nameBuffer, sizeof(nameBuffer), "Entity %u", e.index);
                }
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::InputText("##EntityName", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    ctx->world.SetEntityName(e, nameBuffer);
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    ctx->world.SetEntityName(e, nameBuffer);
                }
            }
            ImGui::Separator();

            std::vector<std::string> componentNames = ctx->world.GetComponentNames(ctx->selectedEntity);
            for (const std::string& compName : componentNames) {
                ShowComponent(ctx, compName);
            }

            ImGui::Separator();
            ImGui::PushID("Inspector");
            if (ImGui::Button("Add Component")) {
                ImGui::OpenPopup("AddComponent");
            }
            ShowAddComponentPopup(ctx);
            ImGui::PopID();
        }
    }
    ImGui::End();
}

} // namespace openeye::editor::panels
