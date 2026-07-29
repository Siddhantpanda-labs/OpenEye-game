#pragma once

#include "api/world/All.h"

#include <cstdint>
#include <string>
#include <unordered_map>

#include "eyelib/math/AABB.h"

namespace eyelib { namespace api { class AssetRegistry; } }
namespace openeye { namespace player {
class RendererGPUResourceCacheNew;
class BuiltinResourceManagerNew;
} }

namespace openeye::editor {

/**
 * Persistent editor-side drawable resource cache.
 *
 * Keyed by entity identity + drawable resource identity so scene build can reuse
 * resolved GPU handles across frames instead of repeating asset-id lookup work.
 */
class EditorPersistentSceneCache {
public:
    struct DrawableKey {
        ::eyelib::world::EntityId entity = ::eyelib::world::InvalidEntity;
        int32_t submeshIndex = 0;
        uint32_t layerFlags = 0;

        bool operator==(const DrawableKey& rhs) const noexcept;
    };

    struct ResolvedDrawableHandles {
        uint32_t meshHandle = 0;
        uint32_t materialId = 0;
        eyelib::math::AABB bounds{};
    };

    void BeginFrame(uint64_t frameSerial);
    void EndFrame();
    void InvalidateEntity(::eyelib::world::EntityId entity);

    ResolvedDrawableHandles ResolveDrawableHandles(
        const DrawableKey& key,
        const std::string& meshAssetId,
        const std::string& materialAssetId,
        eyelib::api::AssetRegistry* assetRegistry,
        openeye::player::RendererGPUResourceCacheNew* gpuResolverNew,
        openeye::player::BuiltinResourceManagerNew* builtinManagerNew);

private:
    struct SubmeshEntry {
        int32_t submeshIndex = 0;
        uint32_t layerFlags = 0;
        ResolvedDrawableHandles handles{};
        uint64_t lastTouchedFrame = 0;
    };

    struct EntityCacheSlot {
        uint32_t generation = 0xFFFFFFFF;
        std::vector<SubmeshEntry> submeshes;
    };

    uint64_t m_frameSerial = 0;
    std::vector<EntityCacheSlot> m_entityCache;
};

} // namespace openeye::editor
