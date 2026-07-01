#pragma once

#include "EntityId.h"
#include "ComponentStorage/IComponentStore.h"
#include "ComponentStorage/ComponentStore.h"
#include "ComponentStorage/TransformStore.h"

#include <entt/entt.hpp>

#include <memory>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eyelib {
namespace world {

class EntityRegistry;
class Hierarchy;
class Metadata;
class Tags;

class WorldState {
public:
    WorldState();
    ~WorldState();

    WorldState(const WorldState&) = delete;
    WorldState& operator=(const WorldState&) = delete;
    WorldState(WorldState&&) noexcept = default;
    WorldState& operator=(WorldState&&) noexcept = default;

    EntityRegistry& Entities();
    const EntityRegistry& Entities() const;

    Hierarchy& GraphHierarchy();
    const Hierarchy& GraphHierarchy() const;

    Metadata& GraphMetadata();
    const Metadata& GraphMetadata() const;

    Tags& GraphTags();
    const Tags& GraphTags() const;

    template<typename T>
    ComponentStore<T>& Components() {
        const size_t typeKey = typeid(T).hash_code();
        auto it = m_componentStores.find(typeKey);
        if (it == m_componentStores.end()) {
            auto store = std::make_unique<ComponentStore<T>>();
            auto* raw = store.get();
            m_componentStores.emplace(typeKey, std::move(store));
            return *raw;
        }
        return *static_cast<ComponentStore<T>*>(it->second.get());
    }

    template<typename T>
    const ComponentStore<T>& Components() const {
        const size_t typeKey = typeid(T).hash_code();
        auto it = m_componentStores.find(typeKey);
        if (it == m_componentStores.end()) {
            static ComponentStore<T> s_empty;
            return s_empty;
        }
        return *static_cast<const ComponentStore<T>*>(it->second.get());
    }

    /** SoA transform store. Use instead of Components<Transform> for cache-friendly iteration (Unreal-style). */
    TransformStore& GetTransformStore();
    const TransformStore& GetTransformStore() const;

    const std::unordered_map<size_t, std::unique_ptr<IComponentStore>>& ComponentStores() const;
    IComponentStore* FindComponentStore(size_t typeKey);
    const IComponentStore* FindComponentStore(size_t typeKey) const;
    void RegisterComponentStore(size_t typeKey, std::unique_ptr<IComponentStore> store);

    void Clear();

    /** Scene-level: skybox cubemap path/GUID, or skybox::kProceduralId for procedural sky. Canonicalized in SetSkyboxAssetId. */
    const std::string& GetSkyboxAssetId() const { return m_skyboxAssetId; }
    void SetSkyboxAssetId(std::string id);
    /** Scene-level: render pipeline mode. Values: Forward | Deferred | ForwardPlus. Serialized in scene file. */
    const std::string& GetRenderPipelineMode() const { return m_renderPipelineMode; }
    void SetRenderPipelineMode(std::string mode) { m_renderPipelineMode = std::move(mode); }
 
    float GetIblIntensity() const { return m_iblIntensity; }
    void SetIblIntensity(float intensity) { m_iblIntensity = intensity; }

    /** Internal only: EnTT registry for component storage. Only Command::Apply and world internals may use this. Never expose in api::world. */
    entt::registry& GetRegistry() { return m_registry; }
    const entt::registry& GetRegistry() const { return m_registry; }

    /** Map World EntityId to EnTT entity. Returns entt::null if entity was not created via registry path. Internal only. */
    entt::entity ToEnTT(EntityId id) const;
    /** Register mapping after creating an entity in both EntityRegistry and registry. Internal only. */
    void RegisterEntityMapping(EntityId id, entt::entity e);
    /** Remove mapping and destroy the EnTT entity. Call before EntityRegistry::Destroy. Internal only. */
    void UnregisterEntity(EntityId id);
    /** True if this entity has an EnTT mapping (e.g. for typed component API). */
    bool HasEnTTEntity(EntityId id) const;
    /** Map EnTT entity back to World EntityId. Returns InvalidEntity if not found. Internal only. */
    EntityId FromEnTT(entt::entity e) const;

private:
    std::unique_ptr<EntityRegistry> m_entities;
    std::unique_ptr<Hierarchy> m_hierarchy;
    std::unique_ptr<Metadata> m_metadata;
    std::unique_ptr<Tags> m_tags;

    std::unordered_map<size_t, std::unique_ptr<IComponentStore>> m_componentStores;
    std::string m_skyboxAssetId;
    std::string m_renderPipelineMode = "Hybrid";
    float m_iblIntensity = 1.0f;
    entt::registry m_registry;
    std::unordered_map<EntityId, entt::entity, EntityIdHash> m_entityToEnTT;
    std::unordered_map<entt::entity, EntityId> m_enttToEntity;
};

} // namespace world
} // namespace eyelib
