#include "EditorSceneDirtyTracker.h"

#include <string>

namespace openeye::editor {

namespace {

inline bool Vec3Equal(const eyelib::math::Vec3& a, const eyelib::math::Vec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool QuatEqual(const eyelib::math::Quat& a, const eyelib::math::Quat& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

inline bool TransformEqual(const ::eyelib::world::components::Transform& a, const ::eyelib::world::components::Transform& b) {
    return Vec3Equal(a.position, b.position) && QuatEqual(a.rotation, b.rotation) && Vec3Equal(a.scale, b.scale);
}

inline bool RenderableEqual(const ::eyelib::world::components::Renderable& a, const ::eyelib::world::components::Renderable& b) {
    if (a.meshAssetId != b.meshAssetId || a.materialAssetId != b.materialAssetId ||
        a.submeshIndex != b.submeshIndex || a.visible != b.visible ||
        a.activeLOD != b.activeLOD || a.lodMeshAssetIds.size() != b.lodMeshAssetIds.size()) {
        return false;
    }
    for (size_t i = 0; i < a.lodMeshAssetIds.size(); ++i) {
        if (a.lodMeshAssetIds[i] != b.lodMeshAssetIds[i]) return false;
    }
    return true;
}

inline bool MeshRendererEqual(const ::eyelib::world::components::MeshRenderer& a, const ::eyelib::world::components::MeshRenderer& b) {
    return a.meshAssetId == b.meshAssetId && a.materialAssetId == b.materialAssetId &&
           a.submeshIndex == b.submeshIndex && a.visible == b.visible;
}

inline bool LightEqual(const ::eyelib::world::components::Light& a, const ::eyelib::world::components::Light& b) {
    return a.type == b.type && Vec3Equal(a.color, b.color) && a.intensity == b.intensity &&
           a.range == b.range && a.innerConeAngle == b.innerConeAngle &&
           a.outerConeAngle == b.outerConeAngle && a.castShadows == b.castShadows;
}

} // namespace

void EditorSceneDirtyTracker::BeginFrame(const eyelib::api::world::World::RenderSnapshot& snapshot) {
    m_dirtyEntities.clear();
    m_cacheInvalidationEntities.clear();
    std::fill(m_dirtyFlagsByEntity.begin(), m_dirtyFlagsByEntity.end(), 0);

    m_frameCount++;

    auto markDirty = [&](EntityId entity, uint32_t flags) {
        if (entity.index >= m_dirtyFlagsByEntity.size()) {
            m_dirtyFlagsByEntity.resize(entity.index + 1, 0);
        }
        if (m_dirtyFlagsByEntity[entity.index] == 0) {
            m_dirtyEntities.push_back(entity);
        }
        m_dirtyFlagsByEntity[entity.index] |= flags;
    };

    auto ensureSize = [](auto& vec, size_t index) {
        if (index >= vec.size()) vec.resize(index + 1);
    };

    for (const auto& entry : snapshot.transforms) {
        const uint32_t idx = entry.entity.index;
        ensureSize(m_prevTransforms, idx);
        auto& state = m_prevTransforms[idx];
        if (state.generation != entry.entity.generation) {
            state.component = entry.component;
            state.generation = entry.entity.generation;
            state.lastVisitedFrame = m_frameCount;
            markDirty(entry.entity, TransformDirty);
        } else {
            state.lastVisitedFrame = m_frameCount;
            if (!TransformEqual(state.component, entry.component)) {
                state.component = entry.component;
                markDirty(entry.entity, TransformDirty);
            }
        }
    }

    for (const auto& entry : snapshot.renderables) {
        const uint32_t idx = entry.entity.index;
        ensureSize(m_prevRenderables, idx);
        auto& state = m_prevRenderables[idx];
        if (state.generation != entry.entity.generation) {
            state.component = entry.component;
            state.generation = entry.entity.generation;
            state.lastVisitedFrame = m_frameCount;
            markDirty(entry.entity, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
        } else {
            state.lastVisitedFrame = m_frameCount;
            if (!RenderableEqual(state.component, entry.component)) {
                state.component = entry.component;
                markDirty(entry.entity, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
            }
        }
    }

    for (const auto& entry : snapshot.meshRenderers) {
        const uint32_t idx = entry.entity.index;
        ensureSize(m_prevMeshRenderers, idx);
        auto& state = m_prevMeshRenderers[idx];
        if (state.generation != entry.entity.generation) {
            state.component = entry.component;
            state.generation = entry.entity.generation;
            state.lastVisitedFrame = m_frameCount;
            markDirty(entry.entity, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
        } else {
            state.lastVisitedFrame = m_frameCount;
            if (!MeshRendererEqual(state.component, entry.component)) {
                state.component = entry.component;
                markDirty(entry.entity, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
            }
        }
    }

    for (const auto& entry : snapshot.lights) {
        const uint32_t idx = entry.entity.index;
        ensureSize(m_prevLights, idx);
        auto& state = m_prevLights[idx];
        if (state.generation != entry.entity.generation) {
            state.component = entry.component;
            state.generation = entry.entity.generation;
            state.lastVisitedFrame = m_frameCount;
            markDirty(entry.entity, LightDirty | VisibilityDirty);
        } else {
            state.lastVisitedFrame = m_frameCount;
            if (!LightEqual(state.component, entry.component)) {
                state.component = entry.component;
                markDirty(entry.entity, LightDirty | VisibilityDirty);
            }
        }
    }

    // Prune stale components and mark dirty
    for (uint32_t i = 0; i < m_prevTransforms.size(); ++i) {
        auto& state = m_prevTransforms[i];
        if (state.generation != 0xFFFFFFFF && state.lastVisitedFrame != m_frameCount) {
            markDirty({i, state.generation}, TransformDirty | VisibilityDirty);
            state.generation = 0xFFFFFFFF;
        }
    }
    for (uint32_t i = 0; i < m_prevRenderables.size(); ++i) {
        auto& state = m_prevRenderables[i];
        if (state.generation != 0xFFFFFFFF && state.lastVisitedFrame != m_frameCount) {
            markDirty({i, state.generation}, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
            state.generation = 0xFFFFFFFF;
        }
    }
    for (uint32_t i = 0; i < m_prevMeshRenderers.size(); ++i) {
        auto& state = m_prevMeshRenderers[i];
        if (state.generation != 0xFFFFFFFF && state.lastVisitedFrame != m_frameCount) {
            markDirty({i, state.generation}, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
            state.generation = 0xFFFFFFFF;
        }
    }
    for (uint32_t i = 0; i < m_prevLights.size(); ++i) {
        auto& state = m_prevLights[i];
        if (state.generation != 0xFFFFFFFF && state.lastVisitedFrame != m_frameCount) {
            markDirty({i, state.generation}, LightDirty | VisibilityDirty);
            state.generation = 0xFFFFFFFF;
        }
    }

    for (const auto& entity : m_dirtyEntities) {
        const uint32_t flags = GetEntityFlags(entity);
        const uint32_t cacheAffecting = RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty;
        if ((flags & cacheAffecting) != 0u)
            m_cacheInvalidationEntities.push_back(entity);
    }
}

uint32_t EditorSceneDirtyTracker::GetEntityFlags(EntityId entity) const {
    if (entity.index >= m_dirtyFlagsByEntity.size()) return None;
    return m_dirtyFlagsByEntity[entity.index];
}

bool EditorSceneDirtyTracker::HasAnyDirty(EntityId entity) const {
    return GetEntityFlags(entity) != None;
}

} // namespace openeye::editor
