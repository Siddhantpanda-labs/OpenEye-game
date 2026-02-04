#pragma once

#include "eyelib/world/ComponentStorage/IComponentStore.h"

#include <json.hpp>

#include <functional>
#include <string>
#include <typeinfo>
#include <unordered_map>

namespace eyelib {
namespace world {

class ComponentSerializer {
public:
    using SerializeStoreFn = std::function<nlohmann::json(const IComponentStore&)>;
    using DeserializeStoreFn = std::function<void(IComponentStore&, const nlohmann::json&)>;
    using SerializeOneFn = std::function<nlohmann::json(const IComponentStore&, EntityId)>;
    using DeserializeOneFn = std::function<void(IComponentStore&, EntityId, const nlohmann::json&)>;
    using CreateStoreFn = std::function<std::unique_ptr<IComponentStore>()>;

    struct Descriptor {
        std::string name;
        uint32_t version = 1;
        size_t typeKey = 0;
        SerializeStoreFn serializeStore;
        DeserializeStoreFn deserializeStore;
        SerializeOneFn serializeOne;
        DeserializeOneFn deserializeOne;
        CreateStoreFn createStore;
    };

    static ComponentSerializer& Get();

    template<typename T>
    void Register(const std::string& name, uint32_t version = 1);

    /** Registers Transform with TransformStore (SoA). Call instead of Register<components::Transform>. */
    void RegisterTransform();

    const Descriptor* GetDescriptor(size_t typeKey) const;
    const Descriptor* GetDescriptorByName(const std::string& name) const;

private:
    ComponentSerializer();
    void RegisterBuiltins();

    std::unordered_map<size_t, Descriptor> m_descriptors;
    std::unordered_map<std::string, size_t> m_nameToType;
};

} // namespace world
} // namespace eyelib

#include "WorldSerializationTraits.h"
#include "eyelib/world/ComponentStorage/ComponentStore.h"

namespace eyelib {
namespace world {

template<typename T>
void ComponentSerializer::Register(const std::string& name, uint32_t version) {
    const size_t typeKey = typeid(T).hash_code();

    Descriptor desc;
    desc.name = name;
    desc.version = version;
    desc.typeKey = typeKey;
    desc.serializeStore = [](const IComponentStore& store) {
        const auto& typed = static_cast<const ComponentStore<T>&>(store);
        nlohmann::json out = nlohmann::json::array();
        for (const auto& [entity, value] : typed.All()) {
            nlohmann::json item;
            item["entity"] = { {"index", entity.index}, {"generation", entity.generation} };
            item["data"] = eyelib::serialize::SerializationTraits<T>::Serialize(value);
            out.push_back(std::move(item));
        }
        return out;
    };
    desc.deserializeStore = [](IComponentStore& store, const nlohmann::json& data) {
        auto& typed = static_cast<ComponentStore<T>&>(store);
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
            T value = eyelib::serialize::SerializationTraits<T>::Deserialize(payload, T{});
            typed.Add(id, value);
        }
    };
    desc.serializeOne = [](const IComponentStore& store, EntityId entity) {
        const auto& typed = static_cast<const ComponentStore<T>&>(store);
        const auto* value = typed.TryGet(entity);
        if (!value) {
            return nlohmann::json{};
        }
        return eyelib::serialize::SerializationTraits<T>::Serialize(*value);
    };
    desc.deserializeOne = [](IComponentStore& store, EntityId entity, const nlohmann::json& data) {
        auto& typed = static_cast<ComponentStore<T>&>(store);
        T value = eyelib::serialize::SerializationTraits<T>::Deserialize(data, T{});
        typed.Add(entity, value);
    };
    desc.createStore = []() {
        return std::make_unique<ComponentStore<T>>();
    };

    m_descriptors[typeKey] = std::move(desc);
    m_nameToType[name] = typeKey;
}

} // namespace world
} // namespace eyelib
