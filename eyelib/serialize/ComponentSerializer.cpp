#include "ComponentSerializer.h"

#include "eyelib/ecs/components/All.h"
#include "eyelib/serialize/SerializationTraits.h"
#include "eyelib/assets/AssetManager.h"
#include "eyelib/assets/AssetRegistry.h"
#include "eyelib/assets/AssetTypes.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Material.h"
#include "entt/entt.hpp"

namespace eyelib {
namespace serialize {

ComponentSerializer& ComponentSerializer::Get() {
    static ComponentSerializer instance;
    return instance;
}

ComponentSerializer::ComponentSerializer() {
    RegisterBuiltins();
}

void ComponentSerializer::RegisterSerializer(ecs::ComponentTypeId type, const std::string& name, uint32_t version, Serializer serializer) {
    m_serializers[type] = std::move(serializer);
    m_descriptors[type] = Descriptor{ name, version };
    m_nameToType[name] = type;
}

nlohmann::json ComponentSerializer::SerializeComponent(ecs::ComponentTypeId type, const void* component) const {
    auto it = m_serializers.find(type);
    if (it == m_serializers.end() || !it->second.serialize) return nlohmann::json{};
    return it->second.serialize(component);
}

void* ComponentSerializer::DeserializeComponent(ecs::ComponentTypeId type, const nlohmann::json& json) const {
    auto it = m_serializers.find(type);
    if (it == m_serializers.end() || !it->second.deserialize) return nullptr;
    return it->second.deserialize(json);
}

const ComponentSerializer::Descriptor* ComponentSerializer::GetDescriptor(ecs::ComponentTypeId type) const {
    auto it = m_descriptors.find(type);
    return it != m_descriptors.end() ? &it->second : nullptr;
}

ecs::ComponentTypeId ComponentSerializer::FindTypeId(const std::string& name) const {
    auto it = m_nameToType.find(name);
    return it != m_nameToType.end() ? it->second : ecs::ComponentTypeId{};
}

void ComponentSerializer::RegisterBuiltins() {
    using namespace ecs::components;

    RegisterSerializer(entt::type_hash<Name>::value(), "Name", 1, {
        [](const void* c) {
            const auto* name = static_cast<const Name*>(c);
            nlohmann::json j;
            j["name"] = name->name;
            return j;
        },
        [](const nlohmann::json& j) -> void* {
            auto* out = new Name();
            out->name = j.value("name", std::string{});
            return out;
        }
    });

    RegisterSerializer(entt::type_hash<Transform>::value(), "Transform", 1, {
        [](const void* c) {
            const auto* t = static_cast<const Transform*>(c);
            nlohmann::json j;
            j["position"] = SerializationTraits<math::Vec3>::Serialize(t->position);
            j["rotation"] = SerializationTraits<math::Quat>::Serialize(t->rotation);
            j["scale"] = SerializationTraits<math::Vec3>::Serialize(t->scale);
            return j;
        },
        [](const nlohmann::json& j) -> void* {
            auto* out = new Transform();
            out->position = SerializationTraits<math::Vec3>::Deserialize(j.value("position", nlohmann::json{}), out->position);
            out->rotation = SerializationTraits<math::Quat>::Deserialize(j.value("rotation", nlohmann::json{}), out->rotation);
            out->scale = SerializationTraits<math::Vec3>::Deserialize(j.value("scale", nlohmann::json{}), out->scale);
            out->dirty = true;
            return out;
        }
    });

    RegisterSerializer(entt::type_hash<Renderable>::value(), "Renderable", 2, {
        [](const void* c) {
            const auto* r = static_cast<const Renderable*>(c);
            nlohmann::json j;
            j["visible"] = r->visible;
            j["activeLOD"] = static_cast<int>(r->activeLOD);
            if (r->mesh && r->mesh->HasAssetId()) {
                j["meshAssetId"] = r->mesh->GetAssetId().ToString();
            }
            if (r->material && r->material->HasAssetId()) {
                j["materialAssetId"] = r->material->GetAssetId().ToString();
            }
            if (!r->lodMeshes.empty()) {
                nlohmann::json lods = nlohmann::json::array();
                for (const auto& lod : r->lodMeshes) {
                    if (lod && lod->HasAssetId()) {
                        lods.push_back(lod->GetAssetId().ToString());
                    } else {
                        lods.push_back(std::string{});
                    }
                }
                j["lodMeshAssetIds"] = std::move(lods);
            }
            return j;
        },
        [](const nlohmann::json& j) -> void* {
            auto* out = new Renderable();
            out->visible = j.value("visible", true);
            out->activeLOD = static_cast<size_t>(j.value("activeLOD", 0));

            auto& manager = assets::AssetManager::Get();
            auto& registry = manager.Registry();
            auto loadHandle = [&manager, &registry](const std::string& idStr) -> std::shared_ptr<render::Mesh> {
                if (idStr.empty()) return nullptr;
                const auto id = assets::AssetId::FromString(idStr);
                if (!id.IsValid()) return nullptr;
                registry.LoadAsset(id);
                return manager.Cache().Get<render::Mesh>(id);
            };

            if (j.contains("meshAssetId") && j["meshAssetId"].is_string()) {
                out->mesh = loadHandle(j["meshAssetId"].get<std::string>());
            }
            if (j.contains("materialAssetId") && j["materialAssetId"].is_string()) {
                const auto id = assets::AssetId::FromString(j["materialAssetId"].get<std::string>());
                if (id.IsValid()) {
                    registry.LoadAsset(id);
                    out->material = manager.Cache().Get<render::Material>(id);
                }
            }
            if (j.contains("lodMeshAssetIds") && j["lodMeshAssetIds"].is_array()) {
                for (const auto& lodId : j["lodMeshAssetIds"]) {
                    if (lodId.is_string()) {
                        out->lodMeshes.push_back(loadHandle(lodId.get<std::string>()));
                    }
                }
            }
            return out;
        }
    });

    RegisterSerializer(entt::type_hash<Camera>::value(), "Camera", 1, {
        [](const void* c) {
            const auto* cam = static_cast<const Camera*>(c);
            nlohmann::json j;
            j["isPrimary"] = cam->isPrimary;
            j["isPerspective"] = cam->isPerspective;
            j["fov"] = cam->fov;
            j["aspect"] = cam->aspect;
            j["near"] = cam->nearPlane;
            j["far"] = cam->farPlane;
            j["orthoSize"] = cam->orthoSize;
            return j;
        },
        [](const nlohmann::json& j) -> void* {
            auto* out = new Camera();
            out->isPrimary = j.value("isPrimary", false);
            out->isPerspective = j.value("isPerspective", true);
            out->fov = j.value("fov", 45.0f);
            out->aspect = j.value("aspect", 16.0f / 9.0f);
            out->nearPlane = j.value("near", 0.1f);
            out->farPlane = j.value("far", 1000.0f);
            out->orthoSize = j.value("orthoSize", 10.0f);
            return out;
        }
    });

    RegisterSerializer(entt::type_hash<Light>::value(), "Light", 1, {
        [](const void* c) {
            const auto* l = static_cast<const Light*>(c);
            nlohmann::json j;
            j["type"] = static_cast<int>(l->type);
            j["color"] = SerializationTraits<math::Vec3>::Serialize(l->color);
            j["intensity"] = l->intensity;
            j["range"] = l->range;
            j["innerCone"] = l->innerConeAngle;
            j["outerCone"] = l->outerConeAngle;
            j["castShadows"] = l->castShadows;
            return j;
        },
        [](const nlohmann::json& j) -> void* {
            auto* out = new Light();
            out->type = static_cast<Light::Type>(j.value("type", 0));
            out->color = SerializationTraits<math::Vec3>::Deserialize(j.value("color", nlohmann::json{}), out->color);
            out->intensity = j.value("intensity", 1.0f);
            out->range = j.value("range", 10.0f);
            out->innerConeAngle = j.value("innerCone", 15.0f);
            out->outerConeAngle = j.value("outerCone", 30.0f);
            out->castShadows = j.value("castShadows", true);
            return out;
        }
    });

    RegisterSerializer(entt::type_hash<RigidBody>::value(), "RigidBody", 1, {
        [](const void* c) {
            const auto* rb = static_cast<const RigidBody*>(c);
            nlohmann::json j;
            j["bodyHandle"] = rb->bodyHandle;
            j["shapeHandle"] = rb->shapeHandle;
            j["syncTransform"] = rb->syncTransform;
            return j;
        },
        [](const nlohmann::json& j) -> void* {
            auto* out = new RigidBody();
            out->bodyHandle = j.value("bodyHandle", 0u);
            out->shapeHandle = j.value("shapeHandle", 0u);
            out->syncTransform = j.value("syncTransform", true);
            return out;
        }
    });

    RegisterSerializer(entt::type_hash<Animator>::value(), "Animator", 1, {
        [](const void* /*c*/) {
            nlohmann::json j;
            j["hasAnimator"] = true;
            return j;
        },
        [](const nlohmann::json& /*j*/) -> void* {
            return new Animator();
        }
    });

    RegisterSerializer(entt::type_hash<AABB>::value(), "AABB", 1, {
        [](const void* c) {
            const auto* aabb = static_cast<const AABB*>(c);
            nlohmann::json j;
            j["min"] = SerializationTraits<math::Vec3>::Serialize(aabb->min);
            j["max"] = SerializationTraits<math::Vec3>::Serialize(aabb->max);
            j["isVisible"] = aabb->isVisible;
            return j;
        },
        [](const nlohmann::json& j) -> void* {
            auto* out = new AABB();
            out->min = SerializationTraits<math::Vec3>::Deserialize(j.value("min", nlohmann::json{}), out->min);
            out->max = SerializationTraits<math::Vec3>::Deserialize(j.value("max", nlohmann::json{}), out->max);
            out->isVisible = j.value("isVisible", true);
            return out;
        }
    });
}

} // namespace serialize
} // namespace eyelib
