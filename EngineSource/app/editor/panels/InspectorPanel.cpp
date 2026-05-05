#include "InspectorPanel.h"
#include "InspectorMaterialEditor.h"
#include "EditorContext.h"
#include "ScenePipelineMode.h"
#include "PanelCommon.h"
#include "ui/PhysicsColliderIcons.h"

#include "api/asset/AssetRegistry.h"
#include "api/asset/AssetTypes.h"
#include "api/render/Material.h"
#include "api/world/All.h"
#include "api/world/Commands.h"
#include "eyelib/world/components/Renderable.h"
#include "scripting/ScriptSchema.h"

#include <imgui.h>
#include <json.hpp>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace openeye::editor::panels {

namespace {

using EntityId = ::eyelib::world::EntityId;

/** Must match AssetPanel's material drag payload. */
constexpr const char* kMaterialPayloadType = "OpenEyeMaterial";
struct MaterialDragPayload { char path[512]; };

/** If path is under assetsRoot, return path relative to assetsRoot; else return path as-is. */
static std::string ToMaterialAssetId(const std::string& path, const std::string& assetsRoot) {
    if (assetsRoot.empty() || path.size() < assetsRoot.size()) return path;
    if (path.compare(0, assetsRoot.size(), assetsRoot) != 0) return path;
    if (path.size() == assetsRoot.size()) return "";
    size_t start = assetsRoot.size();
    if (start < path.size() && (path[start] == '/' || path[start] == '\\')) start++;
    return path.substr(start);
}

void ShowRenderableComponent(EditorContext* ctx, eyelib::api::world::World& world, EntityId entity) {
    using Renderable = ::eyelib::world::components::Renderable;
    Renderable r;
    if (!world.GetRenderable(entity, r)) return;
    ImGui::PushID("InspectorRenderable");
    ImGui::PushID(static_cast<int>(entity.index));
    ImGui::PushID(static_cast<int>(entity.generation));

    char meshBuf[512];
    std::snprintf(meshBuf, sizeof(meshBuf), "%.511s", r.meshAssetId.c_str());
    if (ImGui::InputText("Mesh", meshBuf, sizeof(meshBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        r.meshAssetId = meshBuf;
        world.SetRenderable(entity, r);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        r.meshAssetId = meshBuf;
        world.SetRenderable(entity, r);
    }

    char matBuf[512];
    std::snprintf(matBuf, sizeof(matBuf), "%.511s", r.materialAssetId.c_str());
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("Material", matBuf, sizeof(matBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        r.materialAssetId = matBuf;
        world.SetRenderable(entity, r);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        r.materialAssetId = matBuf;
        world.SetRenderable(entity, r);
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kMaterialPayloadType)) {
            if (p->DataSize >= sizeof(MaterialDragPayload)) {
                const auto* payload = static_cast<const MaterialDragPayload*>(p->Data);
                std::string path(payload->path);
                std::string assetId = ctx && !ctx->projectAssetsRoot.empty()
                    ? ToMaterialAssetId(path, ctx->projectAssetsRoot)
                    : path;
                if (!assetId.empty()) {
                    r.materialAssetId = assetId;
                    world.SetRenderable(entity, r);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (!r.materialAssetId.empty() && ctx && ctx->assetRegistry) {
        ctx->assetRegistry->LoadAsset(r.materialAssetId, eyelib::api::AssetType::Material);
        std::shared_ptr<eyelib::api::Material> mat = ctx->assetRegistry->GetMaterial(r.materialAssetId);
        if (mat) {
            if (ImGui::TreeNodeEx("Reflection", ImGuiTreeNodeFlags_DefaultOpen)) {
                bool on = mat->GetReflective();
                if (ImGui::Checkbox("Reflective", &on)) {
                    mat->SetReflective(on);
                }
                if (on) {
                    float strength = mat->GetReflectiveStrength();
                    if (ImGui::SliderFloat("Strength", &strength, 0.01f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
                        mat->SetReflectiveStrength(strength);
                    }
                }
                ImGui::TreePop();
            }
        }
    }

    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
}

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

void ShowScriptComponent(openeye::editor::EditorContext* ctx) {
    if (!ctx) return;
    eyelib::api::world::World& world = ctx->world;
    EntityId entity = ctx->selectedEntity;
    ::eyelib::world::components::Script script;
    if (!world.GetScript(entity, script)) return;

    ImGui::PushID("InspectorScript");
    ImGui::PushID(static_cast<int>(entity.index));
    ImGui::PushID(static_cast<int>(entity.generation));

    using ScriptValue = ::eyelib::world::components::ScriptValue;
    using ScriptValueType = ScriptValue::Type;

    char scriptNameBuf[512];
    std::snprintf(scriptNameBuf, sizeof(scriptNameBuf), "%.511s", script.scriptName.c_str());
    bool scriptNameChanged = false;
    if (ImGui::InputText("Script (path/id)", scriptNameBuf, sizeof(scriptNameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        script.scriptName = scriptNameBuf;
        scriptNameChanged = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        script.scriptName = scriptNameBuf;
        scriptNameChanged = true;
    }
    if (scriptNameChanged) {
        if (!script.scriptName.empty()) {
            auto schema = openeye::player::scripting::GetScriptSchema(script.scriptName, ctx->assetRoots);
            openeye::player::scripting::ApplySchemaDefaults(schema, script.properties);
        }
        world.SetScript(entity, script);
    }

    if (ImGui::Checkbox("Enabled", &script.isEnabled))
        world.SetScript(entity, script);
    if (ImGui::Checkbox("Needs Reload", &script.needsReload))
        world.SetScript(entity, script);

    static std::unordered_map<std::string, std::vector<openeye::player::scripting::ScriptPropertySchemaEntry>> s_schemaCache;
    std::vector<openeye::player::scripting::ScriptPropertySchemaEntry>* schema = nullptr;
    if (!script.scriptName.empty()) {
        auto it = s_schemaCache.find(script.scriptName);
        if (it == s_schemaCache.end()) {
            auto loaded = openeye::player::scripting::GetScriptSchema(script.scriptName, ctx->assetRoots);
            it = s_schemaCache.emplace(script.scriptName, std::move(loaded)).first;
        }
        schema = &it->second;
    }

    if (ImGui::TreeNodeEx("Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!schema || schema->empty()) {
            ImGui::TextUnformatted("Define .schema in script (see docs).");
        } else {
            std::vector<EntityId> entityList;
            bool entityListFetched = false;
            for (const auto& entry : *schema) {
                ScriptValue& val = script.properties[entry.name];
                if (val.type == ScriptValueType::Nil)
                    val = entry.defaultValue;
                ImGui::PushID(entry.name.c_str());
                bool changed = false;
                if (entry.type == "number") {
                    double v = 0.0;
                    if (std::holds_alternative<double>(val.value)) v = std::get<double>(val.value);
                    float f = static_cast<float>(v);
                    if (entry.hasMinMax) {
                        if (ImGui::DragFloat(entry.name.c_str(), &f, 0.01f, static_cast<float>(entry.minVal), static_cast<float>(entry.maxVal)))
                            changed = true;
                    } else {
                        if (ImGui::DragFloat(entry.name.c_str(), &f, 0.01f))
                            changed = true;
                    }
                    if (changed) { val.type = ScriptValueType::Number; val.value = static_cast<double>(f); }
                } else if (entry.type == "string") {
                    std::string s;
                    if (std::holds_alternative<std::string>(val.value)) s = std::get<std::string>(val.value);
                    char buf[512];
                    std::snprintf(buf, sizeof(buf), "%.511s", s.c_str());
                    if (ImGui::InputText(entry.name.c_str(), buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        val.type = ScriptValueType::String;
                        val.value = std::string(buf);
                        changed = true;
                    }
                } else if (entry.type == "boolean") {
                    bool b = false;
                    if (std::holds_alternative<bool>(val.value)) b = std::get<bool>(val.value);
                    if (ImGui::Checkbox(entry.name.c_str(), &b)) {
                        val.type = ScriptValueType::Bool;
                        val.value = b;
                        changed = true;
                    }
                } else if (entry.type == "vec3") {
                    eyelib::math::Vec3 v{};
                    if (std::holds_alternative<eyelib::math::Vec3>(val.value)) v = std::get<eyelib::math::Vec3>(val.value);
                    float f3[3] = { v.x, v.y, v.z };
                    if (ImGui::DragFloat3(entry.name.c_str(), f3, 0.01f)) {
                        val.type = ScriptValueType::Vec3;
                        val.value = eyelib::math::Vec3{ f3[0], f3[1], f3[2] };
                        changed = true;
                    }
                } else if (entry.type == "entity") {
                    if (!entityListFetched) {
                        entityList = world.GetEntities();
                        entityListFetched = true;
                    }
                    EntityId current = ::eyelib::world::InvalidEntity;
                    if (std::holds_alternative<::eyelib::world::EntityId>(val.value))
                        current = std::get<::eyelib::world::EntityId>(val.value);
                    int currentIdx = -1;
                    for (size_t i = 0; i < entityList.size(); ++i) {
                        if (entityList[i].index == current.index && entityList[i].generation == current.generation) {
                            currentIdx = static_cast<int>(i);
                            break;
                        }
                    }
                    std::string preview = "(None)";
                    if (currentIdx >= 0 && world.IsAlive(current)) {
                        std::string name;
                        if (world.GetEntityName(current, name)) preview = name;
                        else preview = "Entity " + std::to_string(current.index);
                    }
                    if (ImGui::BeginCombo(entry.name.c_str(), preview.c_str())) {
                        if (ImGui::Selectable("(None)", currentIdx == -1)) {
                            val.type = ScriptValueType::Entity;
                            val.value = ::eyelib::world::InvalidEntity;
                            changed = true;
                        }
                        for (size_t i = 0; i < entityList.size(); ++i) {
                            EntityId e = entityList[i];
                            if (!world.IsAlive(e)) continue;
                            std::string name;
                            world.GetEntityName(e, name);
                            if (name.empty()) name = "Entity " + std::to_string(e.index);
                            bool selected = (currentIdx >= 0 && entityList[static_cast<size_t>(currentIdx)].index == e.index);
                            if (ImGui::Selectable(name.c_str(), selected)) {
                                val.type = ScriptValueType::Entity;
                                val.value = e;
                                changed = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                if (changed) {
                    world.SetScript(entity, script);
                }
                ImGui::PopID();
            }
        }
        ImGui::TreePop();
    }

    ImGui::Spacing();
    if (ImGui::Button("Remove Script", ImVec2(-1, 0)))
        world.ApplyCommandSync(eyelib::api::world::RemoveComponent<::eyelib::world::components::Script>(entity));

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
    if (componentName == "Script") {
        if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen))
            ShowScriptComponent(ctx);
        return;
    }
    if (componentName == "Renderable") {
        if (ImGui::CollapsingHeader("Renderable", ImGuiTreeNodeFlags_DefaultOpen))
            ShowRenderableComponent(ctx, world, entity);
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
                void* resolvedTex = reinterpret_cast<void*>(static_cast<uintptr_t>(texId));
                if (ctx->resolveImGuiTextureId)
                    resolvedTex = ctx->resolveImGuiTextureId(resolvedTex);
                if (resolvedTex)
                    ImGui::Image(reinterpret_cast<ImTextureID>(resolvedTex), ImVec2(iconSize, iconSize), ImVec2(0, 1), ImVec2(1, 0));
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
        if (iconsOk && s_icons.GetTexture(ColliderIconShape::Mesh)) {
            void* resolvedTex = reinterpret_cast<void*>(static_cast<uintptr_t>(s_icons.GetTexture(ColliderIconShape::Mesh)));
            if (ctx->resolveImGuiTextureId)
                resolvedTex = ctx->resolveImGuiTextureId(resolvedTex);
            if (resolvedTex)
                ImGui::Image(reinterpret_cast<ImTextureID>(resolvedTex), ImVec2(iconSize, iconSize), ImVec2(0, 1), ImVec2(1, 0));
        }
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
            void* resolvedTex = reinterpret_cast<void*>(static_cast<uintptr_t>(texId));
            if (ctx->resolveImGuiTextureId)
                resolvedTex = ctx->resolveImGuiTextureId(resolvedTex);
            if (resolvedTex)
                ImGui::Image(reinterpret_cast<ImTextureID>(resolvedTex), ImVec2(iconSize, iconSize), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
        }
        if (ImGui::Selectable("Rigidbody")) {
            RigidBody rb;
            world.ApplyCommandSync(eyelib::api::world::AddComponent<RigidBody>(entity, rb));
        }
        ImGui::PopID();
        ImGui::TreePop();
    }

    using Script = ::eyelib::world::components::Script;
    ::eyelib::world::components::Script scriptProbe;
    bool hasScript = world.GetScript(entity, scriptProbe);
    if (ImGui::TreeNodeEx("Script", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("Script");
        if (!hasScript && ImGui::Selectable("Script")) {
            Script s;
            s.scriptName = "";
            s.isEnabled = true;
            s.needsReload = false;
            world.ApplyCommandSync(eyelib::api::world::AddComponent<Script>(entity, s));
        }
        if (hasScript)
            ImGui::Selectable("Script (already added)", false);
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
        } else if (!ctx->selectedMaterialAssetId.empty()) {
            ImGui::TextUnformatted("Material asset");
            ImGui::SameLine();
            if (ImGui::SmallButton("Close##MatInspector")) ctx->selectedMaterialAssetId.clear();
            ImGui::Separator();
            openeye::editor::DrawMaterialAssetEditor(ctx);
        } else if (ctx->sceneSelected) {
            ImGui::TextUnformatted("Scene");
            ImGui::Separator();
            const char* modeItems[] = { "Hybrid (GBuffer + forward transparency)", "Forward-only", "Forward+ (clustered forward-only)" };
            std::string sceneMode = ctx->world.GetRenderPipelineMode();
            if (sceneMode == "Deferred")
                sceneMode = "Hybrid";
            int modeIndex = 0;
            if (sceneMode == "Hybrid") modeIndex = 0;
            else if (sceneMode == "ForwardPlus") modeIndex = 2;
            else modeIndex = 1;
            if (ImGui::Combo("Render Pipeline", &modeIndex, modeItems, IM_ARRAYSIZE(modeItems))) {
                const eyelib::api::RenderPipeline::Mode mode =
                    (modeIndex == 0) ? eyelib::api::RenderPipeline::Mode::Hybrid :
                    (modeIndex == 2) ? eyelib::api::RenderPipeline::Mode::ForwardPlus :
                    eyelib::api::RenderPipeline::Mode::Forward;
                openeye::editor::SetScenePipelineMode(*ctx, mode);
            }
            char skyboxBuf[256];
            std::string skyboxId = ctx->world.GetSkyboxAssetId();
            std::snprintf(skyboxBuf, sizeof(skyboxBuf), "%.255s", skyboxId.c_str());
            if (ImGui::InputText("Skybox (procedural | evening | path or folder)", skyboxBuf, sizeof(skyboxBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
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
            // Tags: list + add/remove
            {
                EntityId e = ctx->selectedEntity;
                std::vector<std::string> tags = ctx->world.GetTags(e);
                if (ImGui::TreeNodeEx("Tags", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (size_t i = 0; i < tags.size(); ++i) {
                        ImGui::PushID(static_cast<int>(i));
                        ImGui::TextUnformatted(tags[i].c_str());
                        ImGui::SameLine();
                        if (ImGui::SmallButton("X")) {
                            ctx->world.RemoveTag(e, tags[i]);
                        }
                        ImGui::PopID();
                    }
                    static char tagBuf[128] = "";
                    ImGui::SetNextItemWidth(120.0f);
                    if (ImGui::InputText("##NewTag", tagBuf, sizeof(tagBuf), ImGuiInputTextFlags_EnterReturnsTrue) && tagBuf[0] != '\0') {
                        ctx->world.AddTag(e, tagBuf);
                        tagBuf[0] = '\0';
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Add") && tagBuf[0] != '\0') {
                        ctx->world.AddTag(e, tagBuf);
                        tagBuf[0] = '\0';
                    }
                    ImGui::TreePop();
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
