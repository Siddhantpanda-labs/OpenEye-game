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
    struct TransformState {
        ::eyelib::world::components::Transform component;
        uint32_t generation = 0xFFFFFFFF;
        uint64_t lastVisitedFrame = 0;
    };
    struct RenderableState {
        ::eyelib::world::components::Renderable component;
        uint32_t generation = 0xFFFFFFFF;
        uint64_t lastVisitedFrame = 0;
    };
    struct MeshRendererState {
        ::eyelib::world::components::MeshRenderer component;
        uint32_t generation = 0xFFFFFFFF;
        uint64_t lastVisitedFrame = 0;
    };
    struct LightState {
        ::eyelib::world::components::Light component;
        uint32_t generation = 0xFFFFFFFF;
        uint64_t lastVisitedFrame = 0;
    };

    std::vector<TransformState> m_prevTransforms;
    std::vector<RenderableState> m_prevRenderables;
    std::vector<MeshRendererState> m_prevMeshRenderers;
    std::vector<LightState> m_prevLights;

    uint64_t m_frameCount = 0;

    std::vector<uint32_t> m_dirtyFlagsByEntity;
    std::vector<EntityId> m_dirtyEntities;
    std::vector<EntityId> m_cacheInvalidationEntities;
};

} // namespace openeye::editor

