#pragma once

#include "Command.h"

#include "eyelib/world/EntityRegistry.h"
#include "eyelib/world/WorldGraph/Hierarchy.h"
#include "eyelib/world/WorldGraph/Metadata.h"
#include "eyelib/world/Events/WorldEvents.h"
#include "eyelib/world/components/Region.h"
#include "eyelib/world/components/SystemBindings.h"

#include <algorithm>
#include <json.hpp>

#include "eyelib/world/Serialization/ComponentSerializer.h"
#include "eyelib/world/Serialization/WorldSerializer.h"
#include "eyelib/serialize/SerializationTraits.h"

#include <optional>
#include <string>

namespace eyelib {
namespace world {

class CreateEntityCommand final : public Command {
public:
    explicit CreateEntityCommand(std::string name = "")
        : m_name(std::move(name)) {}

    bool Apply(WorldState& state) override {
        m_entity = state.Entities().Create();
        if (!m_name.empty()) {
            MetadataEntry entry;
            entry.name = m_name;
            state.GraphMetadata().Set(*m_entity, entry);
        }
        return true;
    }

    bool Revert(WorldState& state) override {
        if (!m_entity.has_value()) {
            return false;
        }
        state.Entities().Destroy(*m_entity);
        state.GraphMetadata().Remove(*m_entity);
        state.GraphHierarchy().RemoveParent(*m_entity);
        return true;
    }

    std::string GetName() const override { return "CreateEntity"; }

    const std::string& GetEntityName() const { return m_name; }
    std::optional<EntityId> GetEntity() const { return m_entity; }

private:
    std::string m_name;
    std::optional<EntityId> m_entity;
};

class DestroyEntityCommand final : public Command {
public:
    explicit DestroyEntityCommand(EntityId entity)
        : m_entity(entity) {}

    bool Apply(WorldState& state) override {
        if (!state.Entities().IsAlive(m_entity)) {
            return false;
        }
        state.Entities().Destroy(m_entity);
        state.GraphMetadata().Remove(m_entity);
        state.GraphHierarchy().RemoveParent(m_entity);
        return true;
    }

    bool Revert(WorldState& /*state*/) override {
        // TODO: Require entity snapshot to restore fully.
        return false;
    }

    std::string GetName() const override { return "DestroyEntity"; }

    EntityId GetEntity() const { return m_entity; }

private:
    EntityId m_entity;
};

class ReparentEntityCommand final : public Command {
public:
    ReparentEntityCommand(EntityId child, EntityId parent)
        : m_child(child), m_parent(parent) {}

    bool Apply(WorldState& state) override {
        m_prevParent = state.GraphHierarchy().GetParent(m_child);
        return state.GraphHierarchy().SetParent(m_child, m_parent);
    }

    bool Revert(WorldState& state) override {
        if (m_prevParent.has_value() && *m_prevParent != InvalidEntity) {
            return state.GraphHierarchy().SetParent(m_child, *m_prevParent);
        }
        state.GraphHierarchy().RemoveParent(m_child);
        return true;
    }

    std::string GetName() const override { return "ReparentEntity"; }

    EntityId GetChild() const { return m_child; }
    EntityId GetParent() const { return m_parent; }

private:
    EntityId m_child;
    EntityId m_parent;
    std::optional<EntityId> m_prevParent;
};

class SetNameCommand final : public Command {
public:
    SetNameCommand(EntityId entity, std::string name)
        : m_entity(entity), m_name(std::move(name)) {}

    bool Apply(WorldState& state) override {
        m_prev = state.GraphMetadata().Get(m_entity);
        MetadataEntry entry = m_prev.value_or(MetadataEntry{});
        entry.name = m_name;
        state.GraphMetadata().Set(m_entity, entry);
        return true;
    }

    bool Revert(WorldState& state) override {
        if (m_prev.has_value()) {
            state.GraphMetadata().Set(m_entity, *m_prev);
        } else {
            state.GraphMetadata().Remove(m_entity);
        }
        return true;
    }

    std::string GetName() const override { return "SetName"; }

    const std::string& GetNewName() const { return m_name; }
    EntityId GetEntity() const { return m_entity; }

private:
    EntityId m_entity;
    std::string m_name;
    std::optional<MetadataEntry> m_prev;
};

class CreateRegionCommand final : public Command {
public:
    explicit CreateRegionCommand(components::Region region = {}, std::string name = "Region")
        : m_region(std::move(region)), m_name(std::move(name)) {}

    bool Apply(WorldState& state) override {
        m_entity = state.Entities().Create();
        state.Components<components::Region>().Add(*m_entity, m_region);
        if (!m_name.empty()) {
            MetadataEntry entry;
            entry.name = m_name;
            state.GraphMetadata().Set(*m_entity, entry);
        }
        return true;
    }

    bool Revert(WorldState& state) override {
        if (!m_entity.has_value()) {
            return false;
        }
        state.Components<components::Region>().Remove(*m_entity);
        state.GraphMetadata().Remove(*m_entity);
        state.GraphHierarchy().RemoveParent(*m_entity);
        state.Entities().Destroy(*m_entity);
        return true;
    }

    std::string GetName() const override { return "CreateRegion"; }

    const std::string& GetRegionName() const { return m_name; }
    const components::Region& GetRegionData() const { return m_region; }
    std::optional<EntityId> GetEntity() const { return m_entity; }

private:
    components::Region m_region;
    std::string m_name;
    std::optional<EntityId> m_entity;
};

class SetRegionDataCommand final : public Command {
public:
    SetRegionDataCommand(EntityId entity, components::Region data)
        : m_entity(entity), m_data(std::move(data)) {}

    bool Apply(WorldState& state) override {
        auto& store = state.Components<components::Region>();
        if (auto* existing = store.TryGet(m_entity)) {
            m_prev = *existing;
        }
        store.Add(m_entity, m_data);
        return true;
    }

    bool Revert(WorldState& state) override {
        auto& store = state.Components<components::Region>();
        if (m_prev.has_value()) {
            store.Add(m_entity, *m_prev);
        } else {
            store.Remove(m_entity);
        }
        return true;
    }

    std::string GetName() const override { return "SetRegionData"; }

    const components::Region& GetRegionData() const { return m_data; }
    EntityId GetEntity() const { return m_entity; }

private:
    EntityId m_entity;
    components::Region m_data;
    std::optional<components::Region> m_prev;
};

class AttachRegionRulesCommand final : public Command {
public:
    AttachRegionRulesCommand(EntityId entity, std::vector<std::string> ruleIds)
        : m_entity(entity), m_ruleIds(std::move(ruleIds)) {}

    bool Apply(WorldState& state) override {
        auto& store = state.Components<components::Region>();
        if (auto* existing = store.TryGet(m_entity)) {
            m_prev = existing->ruleIds;
            existing->ruleIds = m_ruleIds;
            return true;
        }
        return false;
    }

    bool Revert(WorldState& state) override {
        auto& store = state.Components<components::Region>();
        if (auto* existing = store.TryGet(m_entity)) {
            existing->ruleIds = m_prev.value_or(std::vector<std::string>{});
            return true;
        }
        return false;
    }

    std::string GetName() const override { return "AttachRegionRules"; }

    const std::vector<std::string>& GetRuleIds() const { return m_ruleIds; }
    EntityId GetEntity() const { return m_entity; }

private:
    EntityId m_entity;
    std::vector<std::string> m_ruleIds;
    std::optional<std::vector<std::string>> m_prev;
};

class EnableSystemInRegionCommand final : public Command {
public:
    EnableSystemInRegionCommand(EntityId entity, std::string systemName, bool enable)
        : m_entity(entity), m_systemName(std::move(systemName)), m_enable(enable) {}

    bool Apply(WorldState& state) override {
        auto& store = state.Components<components::SystemBindings>();
        auto* bindings = store.TryGet(m_entity);
        if (!bindings) {
            bindings = &store.Add(m_entity, components::SystemBindings{});
        }
        m_prev = bindings->enabledSystems;

        auto& list = bindings->enabledSystems;
        if (m_enable) {
            if (std::find(list.begin(), list.end(), m_systemName) == list.end()) {
                list.push_back(m_systemName);
            }
        } else {
            list.erase(std::remove(list.begin(), list.end(), m_systemName), list.end());
        }
        return true;
    }

    bool Revert(WorldState& state) override {
        auto& store = state.Components<components::SystemBindings>();
        auto* bindings = store.TryGet(m_entity);
        if (!bindings) {
            if (!m_prev.has_value()) {
                return true;
            }
            bindings = &store.Add(m_entity, components::SystemBindings{});
        }
        bindings->enabledSystems = m_prev.value_or(std::vector<std::string>{});
        return true;
    }

    std::string GetName() const override { return "EnableSystemInRegion"; }

    const std::string& GetSystemName() const { return m_systemName; }
    bool GetEnable() const { return m_enable; }
    EntityId GetEntity() const { return m_entity; }

private:
    EntityId m_entity;
    std::string m_systemName;
    bool m_enable = true;
    std::optional<std::vector<std::string>> m_prev;
};

enum class ComponentCommandKind {
    Add,
    Remove,
    Set
};

class ComponentCommandBase : public Command {
public:
    ~ComponentCommandBase() override = default;
    virtual EntityId GetEntity() const = 0;
    virtual WorldEventType GetComponentEventType() const = 0;
    virtual ComponentCommandKind GetComponentCommandKind() const = 0;
    virtual const std::string& GetComponentTypeName() const = 0;
    virtual nlohmann::json SerializeData() const = 0;
};

template<typename T>
class AddComponentCommand final : public ComponentCommandBase {
public:
    AddComponentCommand(EntityId entity, const T& value = T())
        : m_entity(entity), m_value(value) {}

    bool Apply(WorldState& state) override {
        auto& store = state.Components<T>();
        if (store.Has(m_entity)) {
            m_prev = *store.TryGet(m_entity);
        }
        store.Add(m_entity, m_value);
        return true;
    }

    bool Revert(WorldState& state) override {
        auto& store = state.Components<T>();
        if (m_prev.has_value()) {
            store.Add(m_entity, *m_prev);
        } else {
            store.Remove(m_entity);
        }
        return true;
    }

    std::string GetName() const override { return "AddComponent"; }

    EntityId GetEntity() const override { return m_entity; }
    WorldEventType GetComponentEventType() const override { return WorldEventType::ComponentAdded; }
    ComponentCommandKind GetComponentCommandKind() const override { return ComponentCommandKind::Add; }
    const std::string& GetComponentTypeName() const override {
        static const std::string name = []() {
            const auto* desc = ComponentSerializer::Get().GetDescriptor(typeid(T).hash_code());
            return desc ? desc->name : std::string{};
        }();
        return name;
    }
    nlohmann::json SerializeData() const override {
        return eyelib::serialize::SerializationTraits<T>::Serialize(m_value);
    }

private:
    EntityId m_entity;
    T m_value{};
    std::optional<T> m_prev;
};

template<typename T>
class RemoveComponentCommand final : public ComponentCommandBase {
public:
    explicit RemoveComponentCommand(EntityId entity)
        : m_entity(entity) {}

    bool Apply(WorldState& state) override {
        auto& store = state.Components<T>();
        if (auto* existing = store.TryGet(m_entity)) {
            m_prev = *existing;
        }
        store.Remove(m_entity);
        return true;
    }

    bool Revert(WorldState& state) override {
        if (!m_prev.has_value()) {
            return true;
        }
        state.Components<T>().Add(m_entity, *m_prev);
        return true;
    }

    std::string GetName() const override { return "RemoveComponent"; }

    EntityId GetEntity() const override { return m_entity; }
    WorldEventType GetComponentEventType() const override { return WorldEventType::ComponentRemoved; }
    ComponentCommandKind GetComponentCommandKind() const override { return ComponentCommandKind::Remove; }
    const std::string& GetComponentTypeName() const override {
        static const std::string name = []() {
            const auto* desc = ComponentSerializer::Get().GetDescriptor(typeid(T).hash_code());
            return desc ? desc->name : std::string{};
        }();
        return name;
    }
    nlohmann::json SerializeData() const override { return nlohmann::json{}; }

private:
    EntityId m_entity;
    std::optional<T> m_prev;
};

template<typename T>
class SetComponentDataCommand final : public ComponentCommandBase {
public:
    SetComponentDataCommand(EntityId entity, const T& value)
        : m_entity(entity), m_value(value) {}

    bool Apply(WorldState& state) override {
        auto& store = state.Components<T>();
        if (auto* existing = store.TryGet(m_entity)) {
            m_prev = *existing;
        }
        store.Add(m_entity, m_value);
        return true;
    }

    bool Revert(WorldState& state) override {
        auto& store = state.Components<T>();
        if (m_prev.has_value()) {
            store.Add(m_entity, *m_prev);
        } else {
            store.Remove(m_entity);
        }
        return true;
    }

    std::string GetName() const override { return "SetComponentData"; }

    EntityId GetEntity() const override { return m_entity; }
    WorldEventType GetComponentEventType() const override { return WorldEventType::ComponentChanged; }
    ComponentCommandKind GetComponentCommandKind() const override { return ComponentCommandKind::Set; }
    const std::string& GetComponentTypeName() const override {
        static const std::string name = []() {
            const auto* desc = ComponentSerializer::Get().GetDescriptor(typeid(T).hash_code());
            return desc ? desc->name : std::string{};
        }();
        return name;
    }
    nlohmann::json SerializeData() const override {
        return eyelib::serialize::SerializationTraits<T>::Serialize(m_value);
    }

private:
    EntityId m_entity;
    T m_value{};
    std::optional<T> m_prev;
};

class AddComponentByNameCommand final : public ComponentCommandBase {
public:
    AddComponentByNameCommand(EntityId entity, std::string typeName, nlohmann::json data)
        : m_entity(entity), m_typeName(std::move(typeName)), m_data(std::move(data)) {}

    bool Apply(WorldState& state) override {
        auto& serializer = ComponentSerializer::Get();
        const auto* desc = serializer.GetDescriptorByName(m_typeName);
        if (!desc || !desc->deserializeOne || !desc->createStore) {
            return false;
        }
        if (!state.FindComponentStore(desc->typeKey)) {
            state.RegisterComponentStore(desc->typeKey, desc->createStore());
        }
        auto* store = state.FindComponentStore(desc->typeKey);
        if (!store) {
            return false;
        }
        if (desc->serializeOne) {
            m_prev = desc->serializeOne(*store, m_entity);
        }
        desc->deserializeOne(*store, m_entity, m_data);
        return true;
    }

    bool Revert(WorldState& state) override {
        auto& serializer = ComponentSerializer::Get();
        const auto* desc = serializer.GetDescriptorByName(m_typeName);
        if (!desc || !desc->deserializeOne || !desc->createStore) {
            return false;
        }
        auto* store = state.FindComponentStore(desc->typeKey);
        if (!store) {
            if (!m_prev.has_value()) {
                return true;
            }
            state.RegisterComponentStore(desc->typeKey, desc->createStore());
            store = state.FindComponentStore(desc->typeKey);
        }
        if (!store) {
            return false;
        }
        if (m_prev.has_value()) {
            desc->deserializeOne(*store, m_entity, *m_prev);
        }
        return true;
    }

    std::string GetName() const override { return "AddComponent"; }
    EntityId GetEntity() const override { return m_entity; }
    WorldEventType GetComponentEventType() const override { return WorldEventType::ComponentAdded; }
    ComponentCommandKind GetComponentCommandKind() const override { return ComponentCommandKind::Add; }
    const std::string& GetComponentTypeName() const override { return m_typeName; }
    nlohmann::json SerializeData() const override { return m_data; }

private:
    EntityId m_entity;
    std::string m_typeName;
    nlohmann::json m_data;
    std::optional<nlohmann::json> m_prev;
};

class RemoveComponentByNameCommand final : public ComponentCommandBase {
public:
    RemoveComponentByNameCommand(EntityId entity, std::string typeName)
        : m_entity(entity), m_typeName(std::move(typeName)) {}

    bool Apply(WorldState& state) override {
        auto& serializer = ComponentSerializer::Get();
        const auto* desc = serializer.GetDescriptorByName(m_typeName);
        if (!desc || !desc->serializeOne) {
            return false;
        }
        auto* store = state.FindComponentStore(desc->typeKey);
        if (!store) {
            return false;
        }
        m_prev = desc->serializeOne(*store, m_entity);
        store->Remove(m_entity);
        return true;
    }

    bool Revert(WorldState& state) override {
        if (!m_prev.has_value()) {
            return true;
        }
        auto& serializer = ComponentSerializer::Get();
        const auto* desc = serializer.GetDescriptorByName(m_typeName);
        if (!desc || !desc->deserializeOne || !desc->createStore) {
            return false;
        }
        if (!state.FindComponentStore(desc->typeKey)) {
            state.RegisterComponentStore(desc->typeKey, desc->createStore());
        }
        auto* store = state.FindComponentStore(desc->typeKey);
        if (!store) {
            return false;
        }
        desc->deserializeOne(*store, m_entity, *m_prev);
        return true;
    }

    std::string GetName() const override { return "RemoveComponent"; }
    EntityId GetEntity() const override { return m_entity; }
    WorldEventType GetComponentEventType() const override { return WorldEventType::ComponentRemoved; }
    ComponentCommandKind GetComponentCommandKind() const override { return ComponentCommandKind::Remove; }
    const std::string& GetComponentTypeName() const override { return m_typeName; }
    nlohmann::json SerializeData() const override { return nlohmann::json{}; }

private:
    EntityId m_entity;
    std::string m_typeName;
    std::optional<nlohmann::json> m_prev;
};

class SetComponentDataByNameCommand final : public ComponentCommandBase {
public:
    SetComponentDataByNameCommand(EntityId entity, std::string typeName, nlohmann::json data)
        : m_entity(entity), m_typeName(std::move(typeName)), m_data(std::move(data)) {}

    bool Apply(WorldState& state) override {
        auto& serializer = ComponentSerializer::Get();
        const auto* desc = serializer.GetDescriptorByName(m_typeName);
        if (!desc || !desc->deserializeOne || !desc->createStore) {
            return false;
        }
        if (!state.FindComponentStore(desc->typeKey)) {
            state.RegisterComponentStore(desc->typeKey, desc->createStore());
        }
        auto* store = state.FindComponentStore(desc->typeKey);
        if (!store) {
            return false;
        }
        if (desc->serializeOne) {
            m_prev = desc->serializeOne(*store, m_entity);
        }
        desc->deserializeOne(*store, m_entity, m_data);
        return true;
    }

    bool Revert(WorldState& state) override {
        auto& serializer = ComponentSerializer::Get();
        const auto* desc = serializer.GetDescriptorByName(m_typeName);
        if (!desc || !desc->deserializeOne || !desc->createStore) {
            return false;
        }
        if (!state.FindComponentStore(desc->typeKey)) {
            state.RegisterComponentStore(desc->typeKey, desc->createStore());
        }
        auto* store = state.FindComponentStore(desc->typeKey);
        if (!store) {
            return false;
        }
        if (m_prev.has_value()) {
            desc->deserializeOne(*store, m_entity, *m_prev);
        } else {
            store->Remove(m_entity);
        }
        return true;
    }

    std::string GetName() const override { return "SetComponentData"; }
    EntityId GetEntity() const override { return m_entity; }
    WorldEventType GetComponentEventType() const override { return WorldEventType::ComponentChanged; }
    ComponentCommandKind GetComponentCommandKind() const override { return ComponentCommandKind::Set; }
    const std::string& GetComponentTypeName() const override { return m_typeName; }
    nlohmann::json SerializeData() const override { return m_data; }

private:
    EntityId m_entity;
    std::string m_typeName;
    nlohmann::json m_data;
    std::optional<nlohmann::json> m_prev;
};

class LoadSceneCommand final : public Command {
public:
    explicit LoadSceneCommand(std::string path)
        : m_path(std::move(path)) {}

    bool Apply(WorldState& state) override {
        WorldSerializer serializer;
        return serializer.Load(state, m_path);
    }

    bool Revert(WorldState& /*state*/) override {
        return false;
    }

    std::string GetName() const override { return "LoadScene"; }

    const std::string& GetPath() const { return m_path; }

private:
    std::string m_path;
};

} // namespace world
} // namespace eyelib
