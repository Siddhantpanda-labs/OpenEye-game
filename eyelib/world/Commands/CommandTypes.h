#pragma once

#include "Command.h"

#include "eyelib/world/EntityRegistry.h"
#include "eyelib/world/WorldGraph/Hierarchy.h"
#include "eyelib/world/WorldGraph/Metadata.h"
#include "eyelib/world/WorldGraph/Tags.h"
#include "eyelib/world/Events/WorldEvents.h"
#include "eyelib/math/TransformUtils.h"
#include "eyelib/world/ComponentStorage/TransformStore.h"
#include "eyelib/world/components/Transform.h"
#include "eyelib/math/Transform.h"
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
        if (m_entity.has_value()) {
            entt::entity e = state.GetRegistry().create();
            state.RegisterEntityMapping(*m_entity, e);
            // Default component: every entity gets a Transform (identity) on creation
            state.GetTransformStore().Add(*m_entity, math::Transform{});
        }
        if (!m_name.empty() && m_entity.has_value()) {
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
        state.GetTransformStore().Remove(*m_entity);
        state.UnregisterEntity(*m_entity);
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
        // Remove from all component stores (Transform, Renderable, Camera, etc.) so entity is fully gone from scene
        for (const auto& [typeKey, store] : state.ComponentStores()) {
            if (store) {
                store->Remove(m_entity);
            }
        }
        state.UnregisterEntity(m_entity);
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
    /** preserveWorldTransform = true (default): child keeps same world transform, then follows parent. */
    ReparentEntityCommand(EntityId child, EntityId parent, bool preserveWorldTransform = true)
        : m_child(child), m_parent(parent), m_preserveWorld(preserveWorldTransform) {}

    bool Apply(WorldState& state) override {
        m_prevParent = state.GraphHierarchy().GetParent(m_child);
        auto& store = state.GetTransformStore();
        if (m_preserveWorld && store.Has(m_child)) {
            if (const auto* ref = store.TryGet(m_child))
                m_prevLocal = math::Transform{ ref->Position(), ref->Rotation(), ref->Scale() };
            const size_t childIdx = store.GetIndex(m_child);
            const auto wPos = store.WorldPositionsSpan();
            const auto wRot = store.WorldRotationsSpan();
            const auto wScale = store.WorldScalesSpan();
            math::Vec3 newLocalPos, newLocalScale;
            math::Quat newLocalRot;
            if (m_parent == InvalidEntity || !store.Has(m_parent)) {
                newLocalPos = wPos[childIdx];
                newLocalRot = wRot[childIdx];
                newLocalScale = wScale[childIdx];
            } else {
                const size_t parentIdx = store.GetIndex(m_parent);
                math::Vec3 invParentPos, invParentScale;
                math::Quat invParentRot;
                math::InverseTRS(wPos[parentIdx], wRot[parentIdx], wScale[parentIdx], invParentPos, invParentRot, invParentScale);
                math::ComposeTRS(invParentPos, invParentRot, invParentScale,
                                wPos[childIdx], wRot[childIdx], wScale[childIdx],
                                newLocalPos, newLocalRot, newLocalScale);
            }
            if (auto* ref = store.TryGet(m_child)) {
                ref->Position() = math::SanitizePosition(newLocalPos);
                ref->Rotation() = math::SanitizeRotation(newLocalRot);
                ref->Scale() = math::SanitizeScale(newLocalScale);
            }
        }
        if (m_parent == InvalidEntity) {
            state.GraphHierarchy().RemoveParent(m_child);
            state.GetTransformStore().MarkSubtreeDirty(state.GraphHierarchy(), m_child);
            return true;
        }
        if (state.GraphHierarchy().SetParent(m_child, m_parent)) {
            state.GetTransformStore().MarkSubtreeDirty(state.GraphHierarchy(), m_child);
            return true;
        }
        return false;
    }

    bool Revert(WorldState& state) override {
        if (m_prevLocal.has_value()) {
            auto& store = state.GetTransformStore();
            if (auto* ref = store.TryGet(m_child)) {
                ref->Position() = m_prevLocal->position;
                ref->Rotation() = m_prevLocal->rotation;
                ref->Scale() = m_prevLocal->scale;
            }
        }
        if (m_prevParent.has_value() && *m_prevParent != InvalidEntity) {
            return state.GraphHierarchy().SetParent(m_child, *m_prevParent);
        }
        state.GraphHierarchy().RemoveParent(m_child);
        return true;
    }

    std::string GetName() const override { return "ReparentEntity"; }

    EntityId GetChild() const { return m_child; }
    EntityId GetParent() const { return m_parent; }
    bool GetPreserveWorld() const { return m_preserveWorld; }

private:
    EntityId m_child;
    EntityId m_parent;
    bool m_preserveWorld = false;
    std::optional<EntityId> m_prevParent;
    std::optional<math::Transform> m_prevLocal;
};

class AddTagCommand final : public Command {
public:
    AddTagCommand(EntityId entity, std::string tag)
        : m_entity(entity), m_tag(std::move(tag)) {}

    bool Apply(WorldState& state) override {
        if (m_tag.empty()) return false;
        state.GraphTags().Add(m_entity, m_tag);
        return true;
    }

    bool Revert(WorldState& state) override {
        state.GraphTags().Remove(m_entity, m_tag);
        return true;
    }

    std::string GetName() const override { return "AddTag"; }
    EntityId GetEntity() const { return m_entity; }
    const std::string& GetTag() const { return m_tag; }

private:
    EntityId m_entity;
    std::string m_tag;
};

class RemoveTagCommand final : public Command {
public:
    RemoveTagCommand(EntityId entity, std::string tag)
        : m_entity(entity), m_tag(std::move(tag)) {}

    bool Apply(WorldState& state) override {
        state.GraphTags().Remove(m_entity, m_tag);
        return true;
    }

    bool Revert(WorldState& /*state*/) override {
        return false;
    }

    std::string GetName() const override { return "RemoveTag"; }
    EntityId GetEntity() const { return m_entity; }
    const std::string& GetTag() const { return m_tag; }

private:
    EntityId m_entity;
    std::string m_tag;
};

class SetMetadataCommand final : public Command {
public:
    SetMetadataCommand(EntityId entity, std::string name, uint32_t flags)
        : m_entity(entity), m_name(std::move(name)), m_flags(flags) {}

    bool Apply(WorldState& state) override {
        m_prev = state.GraphMetadata().Get(m_entity);
        MetadataEntry entry;
        entry.name = m_name;
        entry.flags = m_flags;
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

    std::string GetName() const override { return "SetMetadata"; }
    EntityId GetEntity() const { return m_entity; }
    const std::string& GetNameValue() const { return m_name; }
    uint32_t GetFlags() const { return m_flags; }

private:
    EntityId m_entity;
    std::string m_name;
    uint32_t m_flags;
    std::optional<MetadataEntry> m_prev;
};

class ClearMetadataCommand final : public Command {
public:
    explicit ClearMetadataCommand(EntityId entity) : m_entity(entity) {}

    bool Apply(WorldState& state) override {
        m_prev = state.GraphMetadata().Get(m_entity);
        state.GraphMetadata().Remove(m_entity);
        return true;
    }

    bool Revert(WorldState& state) override {
        if (m_prev.has_value()) {
            state.GraphMetadata().Set(m_entity, *m_prev);
            return true;
        }
        return true;
    }

    std::string GetName() const override { return "ClearMetadata"; }
    EntityId GetEntity() const { return m_entity; }

private:
    EntityId m_entity;
    std::optional<MetadataEntry> m_prev;
};

class SetTransformCommand final : public Command {
public:
    SetTransformCommand(EntityId entity, const components::Transform& value)
        : m_entity(entity), m_value(value) {}

    bool Apply(WorldState& state) override {
        auto& store = state.GetTransformStore();
        const math::Transform safe{ math::SanitizePosition(m_value.position), math::SanitizeRotation(m_value.rotation), math::SanitizeScale(m_value.scale) };
        if (auto* ref = store.TryGet(m_entity)) {
            *ref = safe;
            store.MarkLocalDirty(m_entity);
            return true;
        }
        store.Add(m_entity, safe);
        return true;
    }

    bool Revert(WorldState& /*state*/) override {
        return false;
    }

    std::string GetName() const override { return "SetTransform"; }
    EntityId GetEntity() const { return m_entity; }
    const components::Transform& GetValue() const { return m_value; }

private:
    EntityId m_entity;
    components::Transform m_value;
};

/** Sets entity's world transform by converting world → local and storing local. Keeps world = parentWorld ⊗ local. */
class SetWorldTransformCommand final : public Command {
public:
    SetWorldTransformCommand(EntityId entity, const components::Transform& worldValue)
        : m_entity(entity), m_worldValue(worldValue) {}

    bool Apply(WorldState& state) override {
        auto& store = state.GetTransformStore();
        const auto& hierarchy = state.GraphHierarchy();
        const math::Vec3 worldPos = math::SanitizePosition(m_worldValue.position);
        const math::Quat worldRot = math::SanitizeRotation(m_worldValue.rotation);
        const math::Vec3 worldScale = math::SanitizeScale(m_worldValue.scale);

        const EntityId parent = hierarchy.GetParent(m_entity);
        const bool hasParentInStore = parent != InvalidEntity && store.Has(parent);

        math::Vec3 localPos, localScale;
        math::Quat localRot;
        if (!hasParentInStore) {
            localPos = worldPos;
            localRot = worldRot;
            localScale = worldScale;
        } else {
            math::Vec3 parentWorldPos, parentWorldScale;
            math::Quat parentWorldRot;
            if (!store.GetWorldTRS(parent, parentWorldPos, parentWorldRot, parentWorldScale)) {
                localPos = worldPos;
                localRot = worldRot;
                localScale = worldScale;
            } else {
                math::Vec3 invPos, invScale;
                math::Quat invRot;
                math::InverseTRS(parentWorldPos, parentWorldRot, parentWorldScale, invPos, invRot, invScale);
                math::ComposeTRS(invPos, invRot, invScale, worldPos, worldRot, worldScale, localPos, localRot, localScale);
                localPos = math::SanitizePosition(localPos);
                localRot = math::SanitizeRotation(localRot);
                localScale = math::SanitizeScale(localScale);
            }
        }

        const math::Transform local{ localPos, localRot, localScale };
        if (auto* ref = store.TryGet(m_entity)) {
            *ref = local;
            store.MarkLocalDirty(m_entity);
            return true;
        }
        store.Add(m_entity, local);
        return true;
    }

    bool Revert(WorldState& /*state*/) override { return false; }

    std::string GetName() const override { return "SetWorldTransform"; }
    EntityId GetEntity() const { return m_entity; }
    const components::Transform& GetWorldValue() const { return m_worldValue; }

private:
    EntityId m_entity;
    components::Transform m_worldValue;
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
        if (m_entity.has_value()) {
            entt::entity e = state.GetRegistry().create();
            state.RegisterEntityMapping(*m_entity, e);
        }
        state.Components<components::Region>().Add(*m_entity, m_region);
        if (!m_name.empty() && m_entity.has_value()) {
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
        state.UnregisterEntity(*m_entity);
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
        if (entt::entity e = state.ToEnTT(m_entity); e != entt::null) {
            state.GetRegistry().emplace_or_replace<T>(e, m_value);
        }
        return true;
    }

    bool Revert(WorldState& state) override {
        auto& store = state.Components<T>();
        if (entt::entity e = state.ToEnTT(m_entity); e != entt::null) {
            state.GetRegistry().remove<T>(e);
            if (m_prev.has_value()) {
                state.GetRegistry().emplace_or_replace<T>(e, *m_prev);
            }
        }
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
        if (entt::entity e = state.ToEnTT(m_entity); e != entt::null) {
            state.GetRegistry().remove<T>(e);
        }
        return true;
    }

    bool Revert(WorldState& state) override {
        if (!m_prev.has_value()) {
            return true;
        }
        state.Components<T>().Add(m_entity, *m_prev);
        if (entt::entity e = state.ToEnTT(m_entity); e != entt::null) {
            state.GetRegistry().emplace_or_replace<T>(e, *m_prev);
        }
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
        if (entt::entity e = state.ToEnTT(m_entity); e != entt::null) {
            state.GetRegistry().emplace_or_replace<T>(e, m_value);
        }
        return true;
    }

    bool Revert(WorldState& state) override {
        auto& store = state.Components<T>();
        if (entt::entity e = state.ToEnTT(m_entity); e != entt::null) {
            state.GetRegistry().remove<T>(e);
            if (m_prev.has_value()) {
                state.GetRegistry().emplace_or_replace<T>(e, *m_prev);
            }
        }
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
