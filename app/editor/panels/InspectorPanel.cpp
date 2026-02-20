#include "InspectorPanel.h"
#include "EditorContext.h"
#include "PanelCommon.h"

#include "api/world/All.h"

#include <imgui.h>
#include <json.hpp>
#include <string>

namespace openeye::editor::panels {

namespace {

using EntityId = ::eyelib::world::EntityId;

void ShowNameComponent(eyelib::api::world::World& world, EntityId entity) {
    std::string jsonStr;
    if (!world.GetComponentJson(entity, "Name", jsonStr)) return;
    nlohmann::json j = nlohmann::json::parse(jsonStr, nullptr, false);
    if (j.is_discarded() || !j.is_string()) return;
    std::string value = j.get<std::string>();
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%.255s", value.c_str());
    if (ImGui::InputText("Name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        nlohmann::json newJ = std::string(buf);
        world.SetComponentJson(entity, "Name", newJ.dump());
        world.SetEntityName(entity, buf);
    }
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
    if (ImGui::InputFloat3("Position", pos)) changed = true;
    if (ImGui::InputFloat4("Rotation (w,x,y,z)", rot)) changed = true;
    if (ImGui::InputFloat3("Scale", scale)) changed = true;

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
    if (!ImGui::CollapsingHeader(componentName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) return;
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
    char idBuf[64];
    std::snprintf(idBuf, sizeof(idBuf), "##%s", componentName.c_str());
    ImGui::InputTextMultiline(idBuf, s_editBuffer.data(), bufSize, ImVec2(-1, 80), ImGuiInputTextFlags_AllowTabInput);
    s_editBuffer.resize(strlen(s_editBuffer.data()));
    if (ImGui::Button("Apply")) {
        if (!s_editBuffer.empty()) {
            nlohmann::json j = nlohmann::json::parse(s_editBuffer, nullptr, false);
            if (!j.is_discarded())
                world.SetComponentJson(entity, componentName, s_editBuffer);
        }
    }
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

    ShowGenericComponent(world, entity, componentName, jsonStr);
}

} // namespace

void ShowInspectorPanel(EditorContext* ctx) {
    if (ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_None)) {
        if (!ctx) {
            ImGui::TextUnformatted("(No context)");
        } else if (ctx->selectedEntity == ::eyelib::world::InvalidEntity || !ctx->world.IsAlive(ctx->selectedEntity)) {
            ImGui::TextUnformatted("Select an entity in Hierarchy.");
        } else {
            std::string name;
            if (ctx->world.GetEntityName(ctx->selectedEntity, name))
                ImGui::Text("Entity: %s", name.c_str());
            else
                ImGui::Text("Entity (index %u)", ctx->selectedEntity.index);
            ImGui::Separator();

            std::vector<std::string> componentNames = ctx->world.GetComponentNames(ctx->selectedEntity);
            for (const std::string& compName : componentNames) {
                ShowComponent(ctx, compName);
            }
        }
    }
    ImGui::End();
}

} // namespace openeye::editor::panels
