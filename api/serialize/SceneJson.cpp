#include "SceneJson.h"

#include "eyelib/core/Log.h"

#include <json.hpp>

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace eyelib {
namespace api {
namespace serialize {

namespace {
static constexpr int kSceneJsonVersion = 1;

static nlohmann::json Vec3ToJson(const eyelib::math::Vec3& v) {
    return nlohmann::json::array({ v.x, v.y, v.z });
}

static nlohmann::json QuatToJson(const eyelib::math::Quat& q) {
    return nlohmann::json::array({ q.w, q.x, q.y, q.z });
}

static eyelib::math::Vec3 Vec3FromJson(const nlohmann::json& j, const eyelib::math::Vec3& fallback) {
    if (!j.is_array() || j.size() != 3) return fallback;
    return eyelib::math::Vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

static eyelib::math::Quat QuatFromJson(const nlohmann::json& j, const eyelib::math::Quat& fallback) {
    if (!j.is_array() || j.size() != 4) return fallback;
    return eyelib::math::Quat(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
}

static nlohmann::json TransformToJson(const eyelib::math::Transform& t) {
    nlohmann::json out;
    out["position"] = Vec3ToJson(t.position);
    out["rotation"] = QuatToJson(t.rotation);
    out["scale"] = Vec3ToJson(t.scale);
    return out;
}

static eyelib::math::Transform TransformFromJson(const nlohmann::json& j) {
    eyelib::math::Transform t{};
    if (!j.is_object()) return t;
    if (j.contains("position")) t.position = Vec3FromJson(j["position"], t.position);
    if (j.contains("rotation")) t.rotation = QuatFromJson(j["rotation"], t.rotation);
    if (j.contains("scale")) t.scale = Vec3FromJson(j["scale"], t.scale);
    return t;
}
} // namespace

std::string SceneJson::ToString(const api::Scene& scene) {
    nlohmann::json root;
    root["version"] = kSceneJsonVersion;
    root["entities"] = nlohmann::json::array();

    const auto entities = scene.GetEntities();
    for (auto e : entities) {
        nlohmann::json ent;
        ent["id"] = e;
        ent["name"] = scene.GetName(e).value_or(std::string{});
        ent["parent"] = scene.GetParent(e);
        ent["transform"] = TransformToJson(scene.GetTransform(e).value_or(eyelib::math::Transform{}));
        root["entities"].push_back(std::move(ent));
    }

    return root.dump(2);
}

bool SceneJson::FromString(api::Scene& scene, const std::string& jsonText) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(jsonText);
    } catch (const std::exception& e) {
        eyelib::core::Log::Error(std::string("api::serialize::SceneJson parse error: ") + e.what());
        return false;
    }

    if (!root.is_object()) return false;
    if (!root.contains("version") || !root["version"].is_number_integer()) return false;
    if (root["version"].get<int>() != kSceneJsonVersion) return false;
    if (!root.contains("entities") || !root["entities"].is_array()) return false;

    scene.Clear();

    std::unordered_map<uint32_t, uint32_t> idMap;
    idMap.reserve(root["entities"].size());

    for (const auto& ent : root["entities"]) {
        if (!ent.is_object()) continue;
        const uint32_t oldId = ent.value("id", 0u);
        const std::string name = ent.value("name", std::string{});
        const auto newEntity = scene.CreateEntity(name);
        idMap[oldId] = newEntity;

        if (ent.contains("transform")) {
            scene.SetTransform(newEntity, TransformFromJson(ent["transform"]));
        }
    }

    for (const auto& ent : root["entities"]) {
        if (!ent.is_object()) continue;
        const uint32_t oldId = ent.value("id", 0u);
        const uint32_t oldParent = ent.value("parent", 0u);
        if (oldId == 0u) continue;

        const auto itChild = idMap.find(oldId);
        if (itChild == idMap.end()) continue;
        const uint32_t newChild = itChild->second;

        if (oldParent == 0u) {
            scene.SetParent(newChild, api::NullEntity);
            continue;
        }

        const auto itParent = idMap.find(oldParent);
        if (itParent == idMap.end()) continue;
        scene.SetParent(newChild, itParent->second);
    }

    return true;
}

bool SceneJson::SaveToFile(const api::Scene& scene, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    const auto text = ToString(scene);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return out.good();
}

bool SceneJson::LoadFromFile(api::Scene& scene, const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    return FromString(scene, ss.str());
}

} // namespace serialize
} // namespace api
} // namespace eyelib

