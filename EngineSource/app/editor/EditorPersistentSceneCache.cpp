#include "EditorPersistentSceneCache.h"

#include "BuiltinResourceManagerNew.h"
#include "RendererGPUResourceCacheNew.h"

#include "api/asset/AssetId.h"
#include "api/asset/AssetRegistry.h"
#include "api/geometry/BuiltinMeshIds.h"

#include <vector>

namespace openeye::editor {

// HashCombine removed as it's no longer needed

void EditorPersistentSceneCache::BeginFrame(uint64_t frameSerial) {
    m_frameSerial = frameSerial;
}

void EditorPersistentSceneCache::EndFrame() {
    for (auto& slot : m_entityCache) {
        if (slot.generation == 0xFFFFFFFF) continue;
        for (auto it = slot.submeshes.begin(); it != slot.submeshes.end(); ) {
            if (it->lastTouchedFrame != m_frameSerial) {
                it = slot.submeshes.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void EditorPersistentSceneCache::InvalidateEntity(::eyelib::world::EntityId entity) {
    if (entity.index < m_entityCache.size()) {
        if (m_entityCache[entity.index].generation == entity.generation) {
            m_entityCache[entity.index].submeshes.clear();
            m_entityCache[entity.index].generation = 0xFFFFFFFF;
        }
    }
}

EditorPersistentSceneCache::ResolvedDrawableHandles EditorPersistentSceneCache::ResolveDrawableHandles(
    const DrawableKey& key,
    const std::string& meshAssetId,
    const std::string& materialAssetId,
    eyelib::api::AssetRegistry* assetRegistry,
    openeye::player::RendererGPUResourceCacheNew* gpuResolverNew,
    openeye::player::BuiltinResourceManagerNew* builtinManagerNew) {
    
    if (key.entity.index >= m_entityCache.size()) {
        m_entityCache.resize(key.entity.index + 1);
    }
    
    auto& slot = m_entityCache[key.entity.index];
    if (slot.generation != key.entity.generation) {
        slot.submeshes.clear();
        slot.generation = key.entity.generation;
    }

    for (auto& sm : slot.submeshes) {
        if (sm.submeshIndex == key.submeshIndex && sm.layerFlags == key.layerFlags) {
            sm.lastTouchedFrame = m_frameSerial;
            return sm.handles;
        }
    }

    ResolvedDrawableHandles handles{};
    if (eyelib::api::geometry::IsBuiltinMeshId(meshAssetId) && builtinManagerNew) {
        auto entry = builtinManagerNew->GetPrimitiveEntry(meshAssetId);
        handles.meshHandle = entry.mesh.GetMeshHandle();
        handles.materialId = entry.material.GetMaterialId();
        handles.bounds = entry.mesh.GetAABB();
    } else if (gpuResolverNew && assetRegistry && !meshAssetId.empty()) {
        const eyelib::api::AssetId mid = assetRegistry->GetId(meshAssetId);
        if (mid.IsValid()) {
            auto mesh = gpuResolverNew->GetMesh(mid);
            handles.meshHandle = mesh.GetMeshHandle();
            handles.bounds = mesh.GetAABB();
        }
    }

    if (!materialAssetId.empty() && gpuResolverNew && assetRegistry) {
        const eyelib::api::AssetId matId = assetRegistry->GetId(materialAssetId);
        if (matId.IsValid())
            handles.materialId = gpuResolverNew->GetMaterial(matId).GetMaterialId();
    }

    if (handles.materialId == 0 && builtinManagerNew)
        handles.materialId = builtinManagerNew->GetDefaultMaterial().GetMaterialId();

    SubmeshEntry newEntry;
    newEntry.submeshIndex = key.submeshIndex;
    newEntry.layerFlags = key.layerFlags;
    newEntry.handles = handles;
    newEntry.lastTouchedFrame = m_frameSerial;
    slot.submeshes.push_back(newEntry);
    
    return handles;
}

} // namespace openeye::editor
