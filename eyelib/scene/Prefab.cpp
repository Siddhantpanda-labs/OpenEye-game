#include "Prefab.h"

#include "eyelib/core/Log.h"
#include "eyelib/ecs/components/All.h"

#include <json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "entt/entt.hpp"

namespace eyelib {
namespace scene {

namespace {
static constexpr int kPrefabJsonVersion = 1;

struct PrefabStore {
    ecs::ComponentRegistry registry;
    std::unordered_map<PrefabHandle, Prefab::PrefabData> prefabs;
    PrefabHandle nextHandle = 1;
    bool initialized = false;

    void EnsureRegistry() {
        if (initialized) return;
        registry.RegisterComponent<ecs::components::Name>();
        registry.RegisterComponent<ecs::components::Transform>();
        registry.RegisterComponent<ecs::components::Renderable>();
        registry.RegisterComponent<ecs::components::Camera>();
        registry.RegisterComponent<ecs::components::Light>();
        registry.RegisterComponent<ecs::components::RigidBody>();
        registry.RegisterComponent<ecs::components::Animator>();
        registry.RegisterComponent<ecs::components::AABB>();
        initialized = true;
    }
};

PrefabStore& Store() {
    static PrefabStore store;
    store.EnsureRegistry();
    return store;
}

static nlohmann::json Vec3ToJson(const math::Vec3& v) {
    return nlohmann::json::array({ v.x, v.y, v.z });
}

static nlohmann::json QuatToJson(const math::Quat& q) {
    return nlohmann::json::array({ q.w, q.x, q.y, q.z });
}

static math::Vec3 Vec3FromJson(const nlohmann::json& j, const math::Vec3& fallback) {
    if (!j.is_array() || j.size() != 3) return fallback;
    return math::Vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

static math::Quat QuatFromJson(const nlohmann::json& j, const math::Quat& fallback) {
    if (!j.is_array() || j.size() != 4) return fallback;
    return math::Quat(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
}

static nlohmann::json TransformToJson(const math::Transform& t) {
    nlohmann::json out;
    out["position"] = Vec3ToJson(t.position);
    out["rotation"] = QuatToJson(t.rotation);
    out["scale"] = Vec3ToJson(t.scale);
    return out;
}

static math::Transform TransformFromJson(const nlohmann::json& j) {
    math::Transform t{};
    if (!j.is_object()) return t;
    if (j.contains("position")) t.position = Vec3FromJson(j["position"], t.position);
    if (j.contains("rotation")) t.rotation = QuatFromJson(j["rotation"], t.rotation);
    if (j.contains("scale")) t.scale = Vec3FromJson(j["scale"], t.scale);
    return t;
}

template<typename T>
std::shared_ptr<void> CloneComponentData(const T& value) {
    return std::shared_ptr<void>(new T(value), [](void* p) { delete static_cast<T*>(p); });
}

static nlohmann::json ComponentToJson(const PrefabComponent& comp, const ecs::ComponentRegistry& registry) {
    nlohmann::json out;
    out["type"] = registry.GetComponentName(comp.type);

    if (comp.type == registry.GetComponentType<ecs::components::Camera>()) {
        const auto* c = static_cast<const ecs::components::Camera*>(comp.data.get());
        out["data"]["isPrimary"] = c->isPrimary;
        out["data"]["isPerspective"] = c->isPerspective;
        out["data"]["fov"] = c->fov;
        out["data"]["aspect"] = c->aspect;
        out["data"]["near"] = c->nearPlane;
        out["data"]["far"] = c->farPlane;
        out["data"]["orthoSize"] = c->orthoSize;
    } else if (comp.type == registry.GetComponentType<ecs::components::Light>()) {
        const auto* l = static_cast<const ecs::components::Light*>(comp.data.get());
        out["data"]["type"] = static_cast<int>(l->type);
        out["data"]["color"] = Vec3ToJson(l->color);
        out["data"]["intensity"] = l->intensity;
        out["data"]["range"] = l->range;
        out["data"]["innerCone"] = l->innerConeAngle;
        out["data"]["outerCone"] = l->outerConeAngle;
        out["data"]["castShadows"] = l->castShadows;
    } else if (comp.type == registry.GetComponentType<ecs::components::AABB>()) {
        const auto* a = static_cast<const ecs::components::AABB*>(comp.data.get());
        out["data"]["min"] = Vec3ToJson(a->min);
        out["data"]["max"] = Vec3ToJson(a->max);
        out["data"]["isVisible"] = a->isVisible;
    } else if (comp.type == registry.GetComponentType<ecs::components::Renderable>()) {
        const auto* r = static_cast<const ecs::components::Renderable*>(comp.data.get());
        out["data"]["visible"] = r->visible;
        out["data"]["activeLOD"] = r->activeLOD;
    } else {
        out["data"] = nullptr;
    }
    return out;
}

static bool ComponentFromJson(const nlohmann::json& compJson, PrefabComponent& out, const ecs::ComponentRegistry& registry) {
    if (!compJson.is_object() || !compJson.contains("type")) return false;
    const std::string typeName = compJson["type"].get<std::string>();
    const auto* data = compJson.contains("data") ? &compJson["data"] : nullptr;

    if (typeName == registry.GetComponentName(registry.GetComponentType<ecs::components::Camera>())) {
        ecs::components::Camera c;
        if (data && data->is_object()) {
            c.isPrimary = data->value("isPrimary", c.isPrimary);
            c.isPerspective = data->value("isPerspective", c.isPerspective);
            c.fov = data->value("fov", c.fov);
            c.aspect = data->value("aspect", c.aspect);
            c.nearPlane = data->value("near", c.nearPlane);
            c.farPlane = data->value("far", c.farPlane);
            c.orthoSize = data->value("orthoSize", c.orthoSize);
        }
        out.type = registry.GetComponentType<ecs::components::Camera>();
        out.data = CloneComponentData(c);
        return true;
    }
    if (typeName == registry.GetComponentName(registry.GetComponentType<ecs::components::Light>())) {
        ecs::components::Light l;
        if (data && data->is_object()) {
            l.type = static_cast<ecs::components::Light::Type>(data->value("type", 0));
            if (data->contains("color")) l.color = Vec3FromJson((*data)["color"], l.color);
            l.intensity = data->value("intensity", l.intensity);
            l.range = data->value("range", l.range);
            l.innerConeAngle = data->value("innerCone", l.innerConeAngle);
            l.outerConeAngle = data->value("outerCone", l.outerConeAngle);
            l.castShadows = data->value("castShadows", l.castShadows);
        }
        out.type = registry.GetComponentType<ecs::components::Light>();
        out.data = CloneComponentData(l);
        return true;
    }
    if (typeName == registry.GetComponentName(registry.GetComponentType<ecs::components::AABB>())) {
        ecs::components::AABB a;
        if (data && data->is_object()) {
            if (data->contains("min")) a.min = Vec3FromJson((*data)["min"], a.min);
            if (data->contains("max")) a.max = Vec3FromJson((*data)["max"], a.max);
            a.isVisible = data->value("isVisible", a.isVisible);
        }
        out.type = registry.GetComponentType<ecs::components::AABB>();
        out.data = CloneComponentData(a);
        return true;
    }
    if (typeName == registry.GetComponentName(registry.GetComponentType<ecs::components::Renderable>())) {
        ecs::components::Renderable r;
        if (data && data->is_object()) {
            r.visible = data->value("visible", r.visible);
            r.activeLOD = static_cast<size_t>(data->value("activeLOD", static_cast<int>(r.activeLOD)));
        }
        out.type = registry.GetComponentType<ecs::components::Renderable>();
        out.data = CloneComponentData(r);
        return true;
    }
    return false;
}

static nlohmann::json NodeToJson(const PrefabNode& node, const ecs::ComponentRegistry& registry) {
    nlohmann::json out;
    out["name"] = node.name;
    out["transform"] = TransformToJson(node.transform);
    out["components"] = nlohmann::json::array();
    for (const auto& comp : node.components) {
        out["components"].push_back(ComponentToJson(comp, registry));
    }
    out["children"] = nlohmann::json::array();
    for (const auto& child : node.children) {
        out["children"].push_back(NodeToJson(child, registry));
    }
    return out;
}

static PrefabNode NodeFromJson(const nlohmann::json& node, const ecs::ComponentRegistry& registry) {
    PrefabNode out;
    if (!node.is_object()) return out;
    out.name = node.value("name", std::string{});
    if (node.contains("transform")) out.transform = TransformFromJson(node["transform"]);
    if (node.contains("components") && node["components"].is_array()) {
        for (const auto& comp : node["components"]) {
            PrefabComponent c;
            if (ComponentFromJson(comp, c, registry)) {
                out.components.push_back(std::move(c));
            }
        }
    }
    if (node.contains("children") && node["children"].is_array()) {
        for (const auto& child : node["children"]) {
            out.children.push_back(NodeFromJson(child, registry));
        }
    }
    return out;
}
} // namespace

ecs::ComponentRegistry& Prefab::Registry() {
    return Store().registry;
}

ecs::ComponentRegistry& Prefab::GetRegistry() {
    return Registry();
}

Prefab::PrefabData* Prefab::GetDataMutable(PrefabHandle handle) {
    auto& map = Store().prefabs;
    auto it = map.find(handle);
    return it != map.end() ? &it->second : nullptr;
}

const Prefab::PrefabData* Prefab::GetData(PrefabHandle handle) {
    auto& map = Store().prefabs;
    auto it = map.find(handle);
    return it != map.end() ? &it->second : nullptr;
}

PrefabHandle Prefab::CreatePrefab(const std::string& name) {
    auto& store = Store();
    const PrefabHandle handle = store.nextHandle++;
    PrefabData data;
    data.name = name;
    data.root.name = name;
    store.prefabs.emplace(handle, std::move(data));
    return handle;
}

PrefabHandle Prefab::CreateFromEntity(Scene& scene, Entity entity, const std::string& name) {
    const PrefabHandle handle = CreatePrefab(name);
    auto* data = GetDataMutable(handle);
    if (!data) return NullPrefab;
    data->root = CaptureNode(scene, entity);
    data->root.name = name;
    return handle;
}

PrefabHandle Prefab::CreateFromHierarchy(Scene& scene, Entity root, const std::string& name) {
    return CreateFromEntity(scene, root, name);
}

bool Prefab::IsValid(PrefabHandle handle) {
    return GetData(handle) != nullptr;
}

void Prefab::DestroyPrefab(PrefabHandle handle) {
    Store().prefabs.erase(handle);
}

const std::string& Prefab::GetName(PrefabHandle handle) {
    static const std::string kEmpty{};
    const auto* data = GetData(handle);
    return data ? data->name : kEmpty;
}

void Prefab::SetName(PrefabHandle handle, const std::string& name) {
    auto* data = GetDataMutable(handle);
    if (!data) return;
    data->name = name;
    data->root.name = name;
}

void Prefab::AddComponent(PrefabHandle handle, ecs::ComponentTypeId type, const void* data) {
    auto* prefab = GetDataMutable(handle);
    if (!prefab) return;
    auto* ops = Registry().GetOps(type);
    if (!ops || !ops->clone || !ops->destroy) return;
    PrefabComponent comp;
    comp.type = type;
    comp.data = std::shared_ptr<void>(ops->clone(data), ops->destroy);
    prefab->root.components.push_back(std::move(comp));
}

void* Prefab::GetComponent(PrefabHandle handle, ecs::ComponentTypeId type) {
    auto* prefab = GetData(handle);
    if (!prefab) return nullptr;
    for (auto& comp : prefab->root.components) {
        if (comp.type == type) return comp.data.get();
    }
    return nullptr;
}

bool Prefab::SavePrefab(PrefabHandle handle, const std::string& path) {
    const auto* data = GetData(handle);
    if (!data) return false;
    nlohmann::json root;
    root["version"] = kPrefabJsonVersion;
    root["name"] = data->name;
    root["root"] = NodeToJson(data->root, Registry());
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    const auto text = root.dump(2);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return out.good();
}

PrefabHandle Prefab::LoadPrefab(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return NullPrefab;
    std::stringstream ss;
    ss << in.rdbuf();
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(ss.str());
    } catch (const std::exception& e) {
        core::Log::Error(std::string("Prefab parse error: ") + e.what());
        return NullPrefab;
    }
    if (!root.is_object()) return NullPrefab;
    if (!root.contains("version") || root["version"].get<int>() != kPrefabJsonVersion) return NullPrefab;
    const std::string name = root.value("name", std::string{});
    const PrefabHandle handle = CreatePrefab(name);
    auto* data = GetDataMutable(handle);
    if (!data) return NullPrefab;
    if (root.contains("root")) {
        data->root = NodeFromJson(root["root"], Registry());
    }
    return handle;
}

const PrefabNode* Prefab::GetRoot(PrefabHandle handle) {
    const auto* data = GetData(handle);
    return data ? &data->root : nullptr;
}

PrefabNode Prefab::CaptureNode(Scene& scene, Entity entity) {
    PrefabNode node;
    node.name = scene.GetName(entity).value_or("Entity");
    node.transform = scene.GetTransform(entity).value_or(math::Transform{});
    CaptureComponents(scene, entity, node);
    const auto children = scene.GetChildren(entity);
    for (Entity child : children) {
        node.children.push_back(CaptureNode(scene, child));
    }
    return node;
}

void Prefab::CaptureComponents(Scene& scene, Entity entity, PrefabNode& node) {
    auto& registry = Registry();
    auto& world = scene.World();
    auto& storage = world.Storage().Registry();
    const auto& types = registry.GetComponentTypes();

    const auto nameId = registry.GetComponentType<ecs::components::Name>();
    const auto transformId = registry.GetComponentType<ecs::components::Transform>();

    for (auto type : types) {
        if (type == nameId || type == transformId) continue;
        const auto* ops = registry.GetOps(type);
        if (!ops || !ops->get || !ops->clone || !ops->destroy) continue;
        void* ptr = ops->get(storage, static_cast<entt::entity>(static_cast<entt::id_type>(entity)));
        if (!ptr) continue;
        PrefabComponent comp;
        comp.type = type;
        comp.data = std::shared_ptr<void>(ops->clone(ptr), ops->destroy);
        node.components.push_back(std::move(comp));
    }
}

} // namespace scene
} // namespace eyelib
