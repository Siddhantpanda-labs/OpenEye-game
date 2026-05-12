#pragma once

#include "api/rendering/RenderDevice.h"
#include "api/rendering/Skybox.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib { namespace api { class AssetRegistry; } }

namespace openeye::player {

/** Runtime cache of `api::rendering::Skybox` entries keyed by `skyboxAssetId`. */
class SkyboxCacheNew {
public:
    SkyboxCacheNew() = default;
    ~SkyboxCacheNew() = default;

    SkyboxCacheNew(const SkyboxCacheNew&) = delete;
    SkyboxCacheNew& operator=(const SkyboxCacheNew&) = delete;

    /** Resolve and cache skybox for `RenderScene::skybox` (non-owning return). */
    eyelib::api::rendering::Skybox* GetOrLoad(eyelib::api::rendering::RenderDevice device,
                                              const std::string& assetId,
                                              const std::vector<std::string>& assetRoots,
                                              eyelib::api::AssetRegistry* assetRegistry,
                                              uint32_t crossFaceSize);

    /** Drop all cached entries (frees GPU resources). Call before device shutdown. */
    void Clear() noexcept { m_cache.clear(); }

private:
    std::unordered_map<std::string, std::unique_ptr<eyelib::api::rendering::Skybox>> m_cache;
};

} // namespace openeye::player
