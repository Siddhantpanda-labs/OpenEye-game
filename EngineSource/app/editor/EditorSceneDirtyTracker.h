#pragma once

#include "api/world/All.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace openeye::editor {

class EditorSceneDirtyTracker {
public:
    enum DirtyFlags : uint32_t {
        None            = 0u,
        TransformDirty  = 1u << 0u,
        RenderableDirty = 1u << 1u,
        MeshDirty       = 1u << 2u,
        MaterialDirty   = 1u << 3u,
        LightDirty      = 1u << 4u,
        VisibilityDirty = 1u << 5u
    };

    void BeginFrame(const eyelib::api::world::World::RenderSnapshot& snapshot);

    uint32_t GetEntityFlags(::eyelib::world::EntityId entity) const;
    bool HasAnyDirty(::eyelib::world::EntityId entity) const;

    const std::vector<::eyelib::world::EntityId>& GetDirtyEntities() const { return m_dirtyEntities; }
    const std::vector<::eyelib::world::EntityId>& GetCacheInvalidationEntities() const { return m_cacheInvalidationEntities; }

private:
    using EntityId = ::eyelib::world::EntityId;
    struct EntityIdHash {
        size_t operator()(const EntityId& id) const noexcept {
            return (static_cast<size_t>(id.generation) << 32u) ^ static_cast<size_t>(id.index);
        }
    };

    void MarkDirty(EntityId entity, uint32_t flags);

    struct TransformState {
        ::eyelib::world::components::Transform component;
        uint64_t lastVisitedFrame = 0;
    };
    struct RenderableState {
        ::eyelib::world::components::Renderable component;
        uint64_t lastVisitedFrame = 0;
    };
    struct MeshRendererState {
        ::eyelib::world::components::MeshRenderer component;
        uint64_t lastVisitedFrame = 0;
    };
    struct LightState {
        ::eyelib::world::components::Light component;
        uint64_t lastVisitedFrame = 0;
    };

    std::unordered_map<EntityId, TransformState, EntityIdHash> m_prevTransforms;
    std::unordered_map<EntityId, RenderableState, EntityIdHash> m_prevRenderables;
    std::unordered_map<EntityId, MeshRendererState, EntityIdHash> m_prevMeshRenderers;
    std::unordered_map<EntityId, LightState, EntityIdHash> m_prevLights;

    uint64_t m_frameCount = 0;

    std::unordered_map<EntityId, uint32_t, EntityIdHash> m_dirtyFlagsByEntity;
    std::vector<EntityId> m_dirtyEntities;
    std::vector<EntityId> m_cacheInvalidationEntities;
};

} // namespace openeye::editor
