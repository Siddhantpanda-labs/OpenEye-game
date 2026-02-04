#include "ComponentSerializer.h"

#include "eyelib/world/ComponentStorage/TransformStore.h"
#include "eyelib/world/components/Transform.h"
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
namespace world {

ComponentSerializer& ComponentSerializer::Get() {
    static ComponentSerializer instance;
    return instance;
}

ComponentSerializer::ComponentSerializer() {
    RegisterBuiltins();
}

void ComponentSerializer::RegisterTransform() {
    const size_t typeKey = typeid(TransformTag).hash_code();
    Descriptor desc;
    desc.name = "Transform";
    desc.version = 1;
    desc.typeKey = typeKey;
    desc.serializeStore = [](const IComponentStore& store) {
        const auto& typed = static_cast<const TransformStore&>(store);
        nlohmann::json out = nlohmann::json::array();
        for (size_t i = 0; i < typed.Size(); ++i) {
            nlohmann::json item;
            item["entity"] = {{"index", typed.m_entityIds[i].index}, {"generation", typed.m_entityIds[i].generation}};
            eyelib::math::Transform t{typed.m_positions[i], typed.m_rotations[i], typed.m_scales[i]};
            item["data"] = eyelib::serialize::SerializationTraits<eyelib::math::Transform>::Serialize(t);
            out.push_back(std::move(item));
        }
        return out;
    };
    desc.deserializeStore = [](IComponentStore& store, const nlohmann::json& data) {
        auto& typed = static_cast<TransformStore&>(store);
        if (!data.is_array()) {
            return;
        }
        for (const auto& item : data) {
            if (!item.is_object() || !item.contains("entity")) {
                continue;
            }
            const auto& ent = item["entity"];
            if (!ent.contains("index") || !ent.contains("generation")) {
                continue;
            }
            EntityId id;
            id.index = ent["index"].get<uint32_t>();
            id.generation = ent["generation"].get<uint32_t>();
            const auto& payload = item.contains("data") ? item["data"] : nlohmann::json{};
            eyelib::math::Transform t =
                eyelib::serialize::SerializationTraits<eyelib::math::Transform>::Deserialize(payload, eyelib::math::Transform{});
            typed.Add(id, t);
        }
    };
    desc.serializeOne = [](const IComponentStore& store, EntityId entity) {
        const auto& typed = static_cast<const TransformStore&>(store);
        const auto* ref = typed.TryGet(entity);
        if (!ref) {
            return nlohmann::json{};
        }
        return eyelib::serialize::SerializationTraits<eyelib::math::Transform>::Serialize(static_cast<eyelib::math::Transform>(*ref));
    };
    desc.deserializeOne = [](IComponentStore& store, EntityId entity, const nlohmann::json& data) {
        auto& typed = static_cast<TransformStore&>(store);
        eyelib::math::Transform t =
            eyelib::serialize::SerializationTraits<eyelib::math::Transform>::Deserialize(data, eyelib::math::Transform{});
        typed.Add(entity, t);
    };
    desc.createStore = []() { return std::make_unique<TransformStore>(); };
    m_descriptors[typeKey] = std::move(desc);
    m_nameToType["Transform"] = typeKey;
}

void ComponentSerializer::RegisterBuiltins() {
    Register<components::Name>("Name", 1);
    RegisterTransform();
    Register<components::Renderable>("Renderable", 1);
    Register<components::Camera>("Camera", 1);
    Register<components::Light>("Light", 1);
    Register<components::RigidBody>("RigidBody", 1);
    Register<components::Animator>("Animator", 1);
    Register<components::AABB>("AABB", 1);
    Register<components::MeshRenderer>("MeshRenderer", 1);
    Register<components::Collider>("Collider", 1);
    Register<components::Script>("Script", 1);
    Register<components::Region>("Region", 1);
    Register<components::SystemBindings>("SystemBindings", 1);
    Register<components::RegionMembership>("RegionMembership", 1);
    Register<components::ActiveSystems>("ActiveSystems", 1);
}

const ComponentSerializer::Descriptor* ComponentSerializer::GetDescriptor(size_t typeKey) const {
    auto it = m_descriptors.find(typeKey);
    return it != m_descriptors.end() ? &it->second : nullptr;
}

const ComponentSerializer::Descriptor* ComponentSerializer::GetDescriptorByName(const std::string& name) const {
    auto it = m_nameToType.find(name);
    if (it == m_nameToType.end()) {
        return nullptr;
    }
    return GetDescriptor(it->second);
}

} // namespace world
} // namespace eyelib
