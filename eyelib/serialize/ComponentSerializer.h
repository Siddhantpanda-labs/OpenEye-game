#pragma once

#include "eyelib/ecs/ComponentRegistry.h"

#include <json.hpp>

#include <functional>
#include <unordered_map>

namespace eyelib {
namespace serialize {

class ComponentSerializer {
public:
    using SerializeFn = std::function<nlohmann::json(const void*)>;
    using DeserializeFn = std::function<void*(const nlohmann::json&)>;

    struct Serializer {
        SerializeFn serialize;
        DeserializeFn deserialize;
    };

    struct Descriptor {
        std::string name;
        uint32_t version = 1;
    };

    static ComponentSerializer& Get();

    void RegisterSerializer(ecs::ComponentTypeId type, const std::string& name, uint32_t version, Serializer serializer);
    nlohmann::json SerializeComponent(ecs::ComponentTypeId type, const void* component) const;
    void* DeserializeComponent(ecs::ComponentTypeId type, const nlohmann::json& json) const;
    const Descriptor* GetDescriptor(ecs::ComponentTypeId type) const;
    ecs::ComponentTypeId FindTypeId(const std::string& name) const;

private:
    ComponentSerializer();
    void RegisterBuiltins();

    std::unordered_map<ecs::ComponentTypeId, Serializer> m_serializers;
    std::unordered_map<ecs::ComponentTypeId, Descriptor> m_descriptors;
    std::unordered_map<std::string, ecs::ComponentTypeId> m_nameToType;
};

} // namespace serialize
} // namespace eyelib
