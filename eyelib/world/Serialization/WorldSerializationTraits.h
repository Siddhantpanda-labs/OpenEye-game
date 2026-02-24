#pragma once

#include "eyelib/serialize/SerializationTraits.h"
#include "eyelib/world/components/Name.h"
#include "eyelib/world/components/Transform.h"
#include "eyelib/world/components/Renderable.h"
#include "eyelib/world/components/Camera.h"
#include "eyelib/world/components/Light.h"
#include "eyelib/world/components/RigidBody.h"
#include "eyelib/world/components/Animator.h"
#include "eyelib/world/components/AABB.h"
#include "eyelib/world/components/MeshRenderer.h"
#include "eyelib/world/components/Collider.h"
#include "eyelib/world/components/Script.h"
#include "eyelib/world/components/Region.h"
#include "eyelib/world/components/SystemBindings.h"
#include "eyelib/world/components/RegionMembership.h"
#include "eyelib/world/components/ActiveSystems.h"

namespace eyelib {
namespace serialize {

template<>
struct SerializationTraits<eyelib::world::components::Name> {
    static nlohmann::json Serialize(const eyelib::world::components::Name& v) { return v.value; }
    static eyelib::world::components::Name Deserialize(const nlohmann::json& j, const eyelib::world::components::Name& fallback = {}) {
        eyelib::world::components::Name out = fallback;
        if (j.is_string()) {
            out.value = j.get<std::string>();
        }
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::Renderable> {
    static nlohmann::json Serialize(const eyelib::world::components::Renderable& v) {
        nlohmann::json j;
        j["meshAssetId"] = v.meshAssetId;
        j["materialAssetId"] = v.materialAssetId;
        j["visible"] = v.visible;
        j["activeLOD"] = v.activeLOD;
        j["lodMeshAssetIds"] = SerializationTraits<std::vector<std::string>>::Serialize(v.lodMeshAssetIds);
        return j;
    }
    static eyelib::world::components::Renderable Deserialize(const nlohmann::json& j, const eyelib::world::components::Renderable& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        out.meshAssetId = j.value("meshAssetId", std::string{});
        out.materialAssetId = j.value("materialAssetId", std::string{});
        out.visible = j.value("visible", true);
        out.activeLOD = j.value("activeLOD", 0);
        if (j.contains("lodMeshAssetIds")) {
            out.lodMeshAssetIds = SerializationTraits<std::vector<std::string>>::Deserialize(j["lodMeshAssetIds"]);
        }
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::Camera> {
    static nlohmann::json Serialize(const eyelib::world::components::Camera& v) {
        nlohmann::json j;
        j["isPrimary"] = v.isPrimary;
        j["isPerspective"] = v.isPerspective;
        j["fov"] = v.fov;
        j["aspect"] = v.aspect;
        j["near"] = v.nearPlane;
        j["far"] = v.farPlane;
        j["orthoSize"] = v.orthoSize;
        return j;
    }
    static eyelib::world::components::Camera Deserialize(const nlohmann::json& j, const eyelib::world::components::Camera& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        out.isPrimary = j.value("isPrimary", false);
        out.isPerspective = j.value("isPerspective", true);
        out.fov = j.value("fov", 45.0f);
        out.aspect = j.value("aspect", 16.0f / 9.0f);
        out.nearPlane = j.value("near", 0.1f);
        out.farPlane = j.value("far", 1000.0f);
        out.orthoSize = j.value("orthoSize", 10.0f);
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::Light> {
    static nlohmann::json Serialize(const eyelib::world::components::Light& v) {
        nlohmann::json j;
        j["type"] = static_cast<int>(v.type);
        j["color"] = SerializationTraits<eyelib::math::Vec3>::Serialize(v.color);
        j["intensity"] = v.intensity;
        j["range"] = v.range;
        j["innerCone"] = v.innerConeAngle;
        j["outerCone"] = v.outerConeAngle;
        j["castShadows"] = v.castShadows;
        return j;
    }
    static eyelib::world::components::Light Deserialize(const nlohmann::json& j, const eyelib::world::components::Light& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        out.type = static_cast<eyelib::world::components::Light::Type>(j.value("type", 0));
        out.color = SerializationTraits<eyelib::math::Vec3>::Deserialize(j.value("color", nlohmann::json{}), out.color);
        out.intensity = j.value("intensity", 1.0f);
        out.range = j.value("range", 10.0f);
        out.innerConeAngle = j.value("innerCone", 15.0f);
        out.outerConeAngle = j.value("outerCone", 30.0f);
        out.castShadows = j.value("castShadows", true);
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::RigidBody> {
    static nlohmann::json Serialize(const eyelib::world::components::RigidBody& v) {
        nlohmann::json j;
        j["bodyHandle"] = v.bodyHandle;
        j["shapeHandle"] = v.shapeHandle;
        j["mass"] = v.mass;
        j["useGravity"] = v.useGravity;
        j["isKinematic"] = v.isKinematic;
        j["syncTransform"] = v.syncTransform;
        return j;
    }
    static eyelib::world::components::RigidBody Deserialize(const nlohmann::json& j, const eyelib::world::components::RigidBody& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        out.bodyHandle = j.value("bodyHandle", 0u);
        out.shapeHandle = j.value("shapeHandle", 0u);
        out.mass = j.value("mass", 1.0f);
        out.useGravity = j.value("useGravity", true);
        out.isKinematic = j.value("isKinematic", false);
        out.syncTransform = j.value("syncTransform", true);
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::Animator> {
    static nlohmann::json Serialize(const eyelib::world::components::Animator& v) {
        nlohmann::json j;
        j["animatorAssetId"] = v.animatorAssetId;
        j["skeletonAssetId"] = v.skeletonAssetId;
        return j;
    }
    static eyelib::world::components::Animator Deserialize(const nlohmann::json& j, const eyelib::world::components::Animator& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        out.animatorAssetId = j.value("animatorAssetId", std::string{});
        out.skeletonAssetId = j.value("skeletonAssetId", std::string{});
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::AABB> {
    static nlohmann::json Serialize(const eyelib::world::components::AABB& v) {
        nlohmann::json j;
        j["min"] = SerializationTraits<eyelib::math::Vec3>::Serialize(v.min);
        j["max"] = SerializationTraits<eyelib::math::Vec3>::Serialize(v.max);
        j["isVisible"] = v.isVisible;
        return j;
    }
    static eyelib::world::components::AABB Deserialize(const nlohmann::json& j, const eyelib::world::components::AABB& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        out.min = SerializationTraits<eyelib::math::Vec3>::Deserialize(j.value("min", nlohmann::json{}), out.min);
        out.max = SerializationTraits<eyelib::math::Vec3>::Deserialize(j.value("max", nlohmann::json{}), out.max);
        out.isVisible = j.value("isVisible", true);
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::MeshRenderer> {
    static nlohmann::json Serialize(const eyelib::world::components::MeshRenderer& v) {
        nlohmann::json j;
        j["meshAssetId"] = v.meshAssetId;
        j["materialAssetId"] = v.materialAssetId;
        j["visible"] = v.visible;
        return j;
    }
    static eyelib::world::components::MeshRenderer Deserialize(const nlohmann::json& j, const eyelib::world::components::MeshRenderer& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        out.meshAssetId = j.value("meshAssetId", std::string{});
        out.materialAssetId = j.value("materialAssetId", std::string{});
        out.visible = j.value("visible", true);
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::Collider> {
    static nlohmann::json Serialize(const eyelib::world::components::Collider& v) {
        nlohmann::json j;
        j["type"] = static_cast<int>(v.type);
        j["center"] = SerializationTraits<eyelib::math::Vec3>::Serialize(v.center);
        j["size"] = SerializationTraits<eyelib::math::Vec3>::Serialize(v.size);
        j["radius"] = v.radius;
        j["height"] = v.height;
        return j;
    }
    static eyelib::world::components::Collider Deserialize(const nlohmann::json& j, const eyelib::world::components::Collider& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        out.type = static_cast<eyelib::world::components::Collider::Type>(j.value("type", 0));
        out.center = SerializationTraits<eyelib::math::Vec3>::Deserialize(j.value("center", nlohmann::json{}), out.center);
        out.size = SerializationTraits<eyelib::math::Vec3>::Deserialize(j.value("size", nlohmann::json{}), out.size);
        out.radius = j.value("radius", 0.5f);
        out.height = j.value("height", 1.0f);
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::ScriptValue> {
    static nlohmann::json Serialize(const eyelib::world::components::ScriptValue& v) {
        nlohmann::json j;
        j["type"] = static_cast<int>(v.type);
        switch (v.type) {
        case eyelib::world::components::ScriptValue::Type::Bool:
            j["value"] = std::get<bool>(v.value);
            break;
        case eyelib::world::components::ScriptValue::Type::Number:
            j["value"] = std::get<double>(v.value);
            break;
        case eyelib::world::components::ScriptValue::Type::String:
            j["value"] = std::get<std::string>(v.value);
            break;
        case eyelib::world::components::ScriptValue::Type::Vec3:
            j["value"] = SerializationTraits<eyelib::math::Vec3>::Serialize(std::get<eyelib::math::Vec3>(v.value));
            break;
        case eyelib::world::components::ScriptValue::Type::Entity: {
            const auto id = std::get<eyelib::world::EntityId>(v.value);
            j["value"] = { {"index", id.index}, {"generation", id.generation} };
            break;
        }
        default:
            j["value"] = nullptr;
            break;
        }
        return j;
    }
    static eyelib::world::components::ScriptValue Deserialize(const nlohmann::json& j, const eyelib::world::components::ScriptValue& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        const int type = j.value("type", 0);
        out.type = static_cast<eyelib::world::components::ScriptValue::Type>(type);
        const auto& payload = j.contains("value") ? j["value"] : nlohmann::json{};
        switch (out.type) {
        case eyelib::world::components::ScriptValue::Type::Bool:
            out.value = payload.is_boolean() ? payload.get<bool>() : false;
            break;
        case eyelib::world::components::ScriptValue::Type::Number:
            out.value = payload.is_number() ? payload.get<double>() : 0.0;
            break;
        case eyelib::world::components::ScriptValue::Type::String:
            out.value = payload.is_string() ? payload.get<std::string>() : std::string{};
            break;
        case eyelib::world::components::ScriptValue::Type::Vec3:
            out.value = SerializationTraits<eyelib::math::Vec3>::Deserialize(payload, {});
            break;
        case eyelib::world::components::ScriptValue::Type::Entity: {
            eyelib::world::EntityId id;
            if (payload.is_object()) {
                id.index = payload.value("index", 0u);
                id.generation = payload.value("generation", 0u);
            }
            out.value = id;
            break;
        }
        default:
            out.value = std::monostate{};
            break;
        }
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::Script> {
    static nlohmann::json Serialize(const eyelib::world::components::Script& v) {
        nlohmann::json j;
        j["scriptName"] = v.scriptName;
        j["isEnabled"] = v.isEnabled;
        j["needsReload"] = v.needsReload;
        nlohmann::json props = nlohmann::json::object();
        for (const auto& [key, value] : v.properties) {
            props[key] = SerializationTraits<eyelib::world::components::ScriptValue>::Serialize(value);
        }
        j["properties"] = std::move(props);
        return j;
    }
    static eyelib::world::components::Script Deserialize(const nlohmann::json& j, const eyelib::world::components::Script& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        out.scriptName = j.value("scriptName", std::string{});
        out.isEnabled = j.value("isEnabled", true);
        out.needsReload = j.value("needsReload", false);
        out.properties.clear();
        if (j.contains("properties") && j["properties"].is_object()) {
            for (auto it = j["properties"].begin(); it != j["properties"].end(); ++it) {
                out.properties[it.key()] = SerializationTraits<eyelib::world::components::ScriptValue>::Deserialize(it.value(), {});
            }
        }
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::Region> {
    static nlohmann::json Serialize(const eyelib::world::components::Region& v) {
        nlohmann::json j;
        j["type"] = static_cast<int>(v.type);
        j["center"] = SerializationTraits<eyelib::math::Vec3>::Serialize(v.center);
        j["size"] = SerializationTraits<eyelib::math::Vec3>::Serialize(v.size);
        j["radius"] = v.radius;
        j["priority"] = v.priority;
        j["enabled"] = v.enabled;
        j["ruleIds"] = SerializationTraits<std::vector<std::string>>::Serialize(v.ruleIds);
        return j;
    }
    static eyelib::world::components::Region Deserialize(const nlohmann::json& j, const eyelib::world::components::Region& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        out.type = static_cast<eyelib::world::components::Region::Type>(j.value("type", 0));
        out.center = SerializationTraits<eyelib::math::Vec3>::Deserialize(j.value("center", nlohmann::json{}), out.center);
        out.size = SerializationTraits<eyelib::math::Vec3>::Deserialize(j.value("size", nlohmann::json{}), out.size);
        out.radius = j.value("radius", 1.0f);
        out.priority = j.value("priority", 0);
        out.enabled = j.value("enabled", true);
        if (j.contains("ruleIds")) {
            out.ruleIds = SerializationTraits<std::vector<std::string>>::Deserialize(j["ruleIds"]);
        }
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::SystemBindings> {
    static nlohmann::json Serialize(const eyelib::world::components::SystemBindings& v) {
        nlohmann::json j;
        j["enabledSystems"] = SerializationTraits<std::vector<std::string>>::Serialize(v.enabledSystems);
        return j;
    }
    static eyelib::world::components::SystemBindings Deserialize(const nlohmann::json& j, const eyelib::world::components::SystemBindings& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        if (j.contains("enabledSystems")) {
            out.enabledSystems = SerializationTraits<std::vector<std::string>>::Deserialize(j["enabledSystems"]);
        }
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::RegionMembership> {
    static nlohmann::json Serialize(const eyelib::world::components::RegionMembership& v) {
        nlohmann::json j;
        j["region"] = { {"index", v.region.index}, {"generation", v.region.generation} };
        return j;
    }
    static eyelib::world::components::RegionMembership Deserialize(const nlohmann::json& j, const eyelib::world::components::RegionMembership& fallback = {}) {
        auto out = fallback;
        if (!j.is_object() || !j.contains("region")) return out;
        const auto& region = j["region"];
        out.region.index = region.value("index", 0u);
        out.region.generation = region.value("generation", 0u);
        return out;
    }
};

template<>
struct SerializationTraits<eyelib::world::components::ActiveSystems> {
    static nlohmann::json Serialize(const eyelib::world::components::ActiveSystems& v) {
        nlohmann::json j;
        j["enabledSystems"] = SerializationTraits<std::vector<std::string>>::Serialize(v.enabledSystems);
        return j;
    }
    static eyelib::world::components::ActiveSystems Deserialize(const nlohmann::json& j, const eyelib::world::components::ActiveSystems& fallback = {}) {
        auto out = fallback;
        if (!j.is_object()) return out;
        if (j.contains("enabledSystems")) {
            out.enabledSystems = SerializationTraits<std::vector<std::string>>::Deserialize(j["enabledSystems"]);
        }
        return out;
    }
};

} // namespace serialize
} // namespace eyelib
