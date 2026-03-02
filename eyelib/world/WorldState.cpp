#include "WorldState.h"

#include "ComponentStorage/TransformStore.h"
#include "EntityRegistry.h"
#include "WorldGraph/Hierarchy.h"
#include "WorldGraph/Metadata.h"
#include "WorldGraph/Tags.h"

#include <typeinfo>

namespace eyelib {
namespace world {

TransformStore& WorldState::GetTransformStore() {
    const size_t typeKey = typeid(TransformTag).hash_code();
    auto it = m_componentStores.find(typeKey);
    if (it == m_componentStores.end()) {
        auto store = std::make_unique<TransformStore>();
        auto* raw = store.get();
        m_componentStores.emplace(typeKey, std::move(store));
        return *raw;
    }
    return *static_cast<TransformStore*>(it->second.get());
}

const TransformStore& WorldState::GetTransformStore() const {
    const size_t typeKey = typeid(TransformTag).hash_code();
    auto it = m_componentStores.find(typeKey);
    if (it == m_componentStores.end()) {
        static TransformStore s_empty;
        return s_empty;
    }
    return *static_cast<const TransformStore*>(it->second.get());
}

WorldState::WorldState()
    : m_entities(std::make_unique<EntityRegistry>()),
      m_hierarchy(std::make_unique<Hierarchy>()),
      m_metadata(std::make_unique<Metadata>()),
      m_tags(std::make_unique<Tags>()) {}

WorldState::~WorldState() = default;

EntityRegistry& WorldState::Entities() { return *m_entities; }
const EntityRegistry& WorldState::Entities() const { return *m_entities; }

Hierarchy& WorldState::GraphHierarchy() { return *m_hierarchy; }
const Hierarchy& WorldState::GraphHierarchy() const { return *m_hierarchy; }

Metadata& WorldState::GraphMetadata() { return *m_metadata; }
const Metadata& WorldState::GraphMetadata() const { return *m_metadata; }

Tags& WorldState::GraphTags() { return *m_tags; }
const Tags& WorldState::GraphTags() const { return *m_tags; }

const std::unordered_map<size_t, std::unique_ptr<IComponentStore>>& WorldState::ComponentStores() const {
    return m_componentStores;
}

IComponentStore* WorldState::FindComponentStore(size_t typeKey) {
    auto it = m_componentStores.find(typeKey);
    if (it == m_componentStores.end()) {
        return nullptr;
    }
    return it->second.get();
}

const IComponentStore* WorldState::FindComponentStore(size_t typeKey) const {
    auto it = m_componentStores.find(typeKey);
    if (it == m_componentStores.end()) {
        return nullptr;
    }
    return it->second.get();
}

void WorldState::RegisterComponentStore(size_t typeKey, std::unique_ptr<IComponentStore> store) {
    if (!store) {
        return;
    }
    m_componentStores[typeKey] = std::move(store);
}

entt::entity WorldState::ToEnTT(EntityId id) const {
    auto it = m_entityToEnTT.find(id);
    if (it == m_entityToEnTT.end()) {
        return entt::null;
    }
    return it->second;
}

void WorldState::RegisterEntityMapping(EntityId id, entt::entity e) {
    m_entityToEnTT[id] = e;
    m_enttToEntity[e] = id;
}

void WorldState::UnregisterEntity(EntityId id) {
    auto it = m_entityToEnTT.find(id);
    if (it == m_entityToEnTT.end()) {
        return;
    }
    entt::entity e = it->second;
    m_enttToEntity.erase(e);
    m_entityToEnTT.erase(it);
    m_registry.destroy(e);
}

bool WorldState::HasEnTTEntity(EntityId id) const {
    return m_entityToEnTT.find(id) != m_entityToEnTT.end();
}

EntityId WorldState::FromEnTT(entt::entity e) const {
    auto it = m_enttToEntity.find(e);
    if (it == m_enttToEntity.end()) {
        return InvalidEntity;
    }
    return it->second;
}

void WorldState::Clear() {
    m_entityToEnTT.clear();
    m_enttToEntity.clear();
    m_registry.clear();
    m_componentStores.clear();
    m_skyboxAssetId.clear();
    if (m_entities) {
        m_entities->Clear();
    }
    if (m_hierarchy) {
        m_hierarchy->Clear();
    }
    if (m_metadata) {
        m_metadata->Clear();
    }
    if (m_tags) {
        m_tags->Clear();
    }
}

} // namespace world
} // namespace eyelib
