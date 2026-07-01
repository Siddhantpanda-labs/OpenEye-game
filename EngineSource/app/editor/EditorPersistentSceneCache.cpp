#include "EditorPersistentSceneCache.h"

#include "BuiltinResourceManagerNew.h"
#include "RendererGPUResourceCacheNew.h"

#include "api/asset/AssetId.h"
#include "api/asset/AssetRegistry.h"
#include "api/geometry/BuiltinMeshIds.h"

#include <vector>

namespace openeye::editor {

namespace {

inline void HashCombine(size_t& seed, size_t value) {
    seed ^= value + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
}

} // namespace

bool EditorPersistentSceneCache::DrawableKey::operator==(const DrawableKey& rhs) const noexcept {
    return entity.index == rhs.entity.index &&
           entity.generation == rhs.entity.generation &&
           submeshIndex == rhs.submeshIndex &&
           layerFlags == rhs.layerFlags;
}

size_t EditorPersistentSceneCache::DrawableKeyHash::operator()(const DrawableKey& key) const noexcept {
    size_t seed = 0;
    HashCombine(seed, std::hash<uint32_t>{}(key.entity.index));
    HashCombine(seed, std::hash<uint32_t>{}(key.entity.generation));
    HashCombine(seed, std::hash<int32_t>{}(key.submeshIndex));
    HashCombine(seed, std::hash<uint32_t>{}(key.layerFlags));
    return seed;
}

void EditorPersistentSceneCache::BeginFrame(uint64_t frameSerial) {
    m_frameSerial = frameSerial;
}

void EditorPersistentSceneCache::EndFrame() {
    if (m_drawableEntries.empty())
        return;
    std::vector<DrawableKey> stale;
    stale.reserve(m_drawableEntries.size() / 4u + 1u);
    for (const auto& [key, entry] : m_drawableEntries) {
        (void)key;
        if (entry.lastTouchedFrame != m_frameSerial)
            stale.push_back(key);
    }
    for (const auto& key : stale)
        m_drawableEntries.erase(key);
}

void EditorPersistentSceneCache::InvalidateEntity(::eyelib::world::EntityId entity) {
    if (m_drawableEntries.empty())
        return;
    std::vector<DrawableKey> toRemove;
    toRemove.reserve(8);
    for (const auto& [key, entry] : m_drawableEntries) {
        (void)entry;
        if (key.entity.index == entity.index && key.entity.generation == entity.generation)
            toRemove.push_back(key);
    }
    for (const auto& key : toRemove)
        m_drawableEntries.erase(key);
}

EditorPersistentSceneCache::ResolvedDrawableHandles EditorPersistentSceneCache::ResolveDrawableHandles(
    const DrawableKey& key,
    const std::string& meshAssetId,
    const std::string& materialAssetId,
    eyelib::api::AssetRegistry* assetRegistry,
    openeye::player::RendererGPUResourceCacheNew* gpuResolverNew,
    openeye::player::BuiltinResourceManagerNew* builtinManagerNew) {
    auto it = m_drawableEntries.find(key);
    if (it != m_drawableEntries.end()) {
        it->second.lastTouchedFrame = m_frameSerial;
        return it->second.handles;
    }

    ResolvedDrawableHandles handles{};
    if (eyelib::api::geometry::IsBuiltinMeshId(meshAssetId) && builtinManagerNew) {
        auto entry = builtinManagerNew->GetPrimitiveEntry(meshAssetId);
        handles.meshHandle = entry.mesh.GetMeshHandle();
        handles.materialId = entry.material.GetMaterialId();
    } else if (gpuResolverNew && assetRegistry && !meshAssetId.empty()) {
        const eyelib::api::AssetId mid = assetRegistry->GetId(meshAssetId);
        if (mid.IsValid())
            handles.meshHandle = gpuResolverNew->GetMesh(mid).GetMeshHandle();
    }

    if (!materialAssetId.empty() && gpuResolverNew && assetRegistry) {
        const eyelib::api::AssetId matId = assetRegistry->GetId(materialAssetId);
        if (matId.IsValid())
            handles.materialId = gpuResolverNew->GetMaterial(matId).GetMaterialId();
    }

    if (handles.materialId == 0 && builtinManagerNew)
        handles.materialId = builtinManagerNew->GetDefaultMaterial().GetMaterialId();

    Entry entry{};
    entry.handles = handles;
    entry.lastTouchedFrame = m_frameSerial;
    m_drawableEntries.emplace(key, entry);
    return handles;
}

} // namespace openeye::editor
