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

void EditorSceneDirtyTracker::MarkDirty(EntityId entity, uint32_t flags) {
    auto it = m_dirtyFlagsByEntity.find(entity);
    if (it == m_dirtyFlagsByEntity.end()) {
        m_dirtyFlagsByEntity.emplace(entity, flags);
        m_dirtyEntities.push_back(entity);
    } else {
        it->second |= flags;
    }
}

void EditorSceneDirtyTracker::BeginFrame(const eyelib::api::world::World::RenderSnapshot& snapshot) {
    m_dirtyFlagsByEntity.clear();
    m_dirtyEntities.clear();
    m_cacheInvalidationEntities.clear();

    m_frameCount++;

    for (const auto& entry : snapshot.transforms) {
        auto it = m_prevTransforms.find(entry.entity);
        if (it == m_prevTransforms.end()) {
            m_prevTransforms.insert({entry.entity, {entry.component, m_frameCount}});
            MarkDirty(entry.entity, TransformDirty);
        } else {
            it->second.lastVisitedFrame = m_frameCount;
            if (!TransformEqual(it->second.component, entry.component)) {
                it->second.component = entry.component;
                MarkDirty(entry.entity, TransformDirty);
            }
        }
    }

    for (const auto& entry : snapshot.renderables) {
        auto it = m_prevRenderables.find(entry.entity);
        if (it == m_prevRenderables.end()) {
            m_prevRenderables.insert({entry.entity, {entry.component, m_frameCount}});
            MarkDirty(entry.entity, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
        } else {
            it->second.lastVisitedFrame = m_frameCount;
            if (!RenderableEqual(it->second.component, entry.component)) {
                it->second.component = entry.component;
                MarkDirty(entry.entity, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
            }
        }
    }

    for (const auto& entry : snapshot.meshRenderers) {
        auto it = m_prevMeshRenderers.find(entry.entity);
        if (it == m_prevMeshRenderers.end()) {
            m_prevMeshRenderers.insert({entry.entity, {entry.component, m_frameCount}});
            MarkDirty(entry.entity, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
        } else {
            it->second.lastVisitedFrame = m_frameCount;
            if (!MeshRendererEqual(it->second.component, entry.component)) {
                it->second.component = entry.component;
                MarkDirty(entry.entity, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
            }
        }
    }

    for (const auto& entry : snapshot.lights) {
        auto it = m_prevLights.find(entry.entity);
        if (it == m_prevLights.end()) {
            m_prevLights.insert({entry.entity, {entry.component, m_frameCount}});
            MarkDirty(entry.entity, LightDirty | VisibilityDirty);
        } else {
            it->second.lastVisitedFrame = m_frameCount;
            if (!LightEqual(it->second.component, entry.component)) {
                it->second.component = entry.component;
                MarkDirty(entry.entity, LightDirty | VisibilityDirty);
            }
        }
    }

    // Prune stale components and mark dirty
    for (auto it = m_prevTransforms.begin(); it != m_prevTransforms.end(); ) {
        if (it->second.lastVisitedFrame != m_frameCount) {
            MarkDirty(it->first, TransformDirty | VisibilityDirty);
            it = m_prevTransforms.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_prevRenderables.begin(); it != m_prevRenderables.end(); ) {
        if (it->second.lastVisitedFrame != m_frameCount) {
            MarkDirty(it->first, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
            it = m_prevRenderables.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_prevMeshRenderers.begin(); it != m_prevMeshRenderers.end(); ) {
        if (it->second.lastVisitedFrame != m_frameCount) {
            MarkDirty(it->first, RenderableDirty | MeshDirty | MaterialDirty | VisibilityDirty);
            it = m_prevMeshRenderers.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_prevLights.begin(); it != m_prevLights.end(); ) {
        if (it->second.lastVisitedFrame != m_frameCount) {
            MarkDirty(it->first, LightDirty | VisibilityDirty);
            it = m_prevLights.erase(it);
        } else {
            ++it;
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
    const auto it = m_dirtyFlagsByEntity.find(entity);
    if (it == m_dirtyFlagsByEntity.end())
        return None;
    return it->second;
}

bool EditorSceneDirtyTracker::HasAnyDirty(EntityId entity) const {
    return GetEntityFlags(entity) != None;
}

} // namespace openeye::editor
