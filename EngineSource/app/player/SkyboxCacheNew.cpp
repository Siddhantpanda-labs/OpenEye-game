#include "SkyboxCacheNew.h"

#include "api/asset/AssetId.h"
#include "api/asset/AssetRegistry.h"
#include "api/core/Log.h"
#include "api/platform/All.h"  // pulls FileSystem + the `eyelib::api::platform::filesystem` alias.

#include "eyelib/world/SkyboxAssetId.h"

#include <array>

namespace openeye::player {

namespace {

std::string JoinPath(const std::string& base, const std::string& leaf) {
    if (base.empty()) return leaf;
    const char last = base.back();
    if (last == '/' || last == '\\') return base + leaf;
    return base + "/" + leaf;
}

std::string ResolveAssetPath(const std::string& id,
                             const std::vector<std::string>& roots,
                             eyelib::api::AssetRegistry* registry) {
    namespace fs = eyelib::api::platform::filesystem;
    if (id.empty()) return {};
    if (fs::FileExists(id)) return id;
    for (const auto& root : roots) {
        const std::string candidate = JoinPath(root, id);
        if (fs::FileExists(candidate)) return candidate;
    }
    if (registry) {
        eyelib::api::AssetId assetId = eyelib::api::AssetId::FromString(id);
        if (assetId.IsValid()) {
            std::string path = registry->GetFilePath(assetId);
            if (!path.empty()) return path;
        }
    }
    return {};
}

} // namespace

eyelib::api::rendering::Skybox* SkyboxCacheNew::GetOrLoad(
    eyelib::api::rendering::RenderDevice device, const std::string& assetId,
    const std::vector<std::string>& assetRoots, eyelib::api::AssetRegistry* assetRegistry,
    uint32_t crossFaceSize) {
    if (!device) return nullptr;

    const std::string normalizedId = eyelib::world::skybox::NormalizeSkyboxAssetId(assetId);

    auto it = m_cache.find(normalizedId);
    if (it != m_cache.end()) return it->second.get();

    auto entry = std::make_unique<eyelib::api::rendering::Skybox>();

    if (eyelib::world::skybox::IsProceduralSkyboxId(normalizedId)) {
        entry->SetProcedural();
        auto* raw = entry.get();
        m_cache.emplace(normalizedId, std::move(entry));
        return raw;
    }

    const std::string resolved = ResolveAssetPath(normalizedId, assetRoots, assetRegistry);
    bool loaded = false;
    if (!resolved.empty()) {
        namespace fs = eyelib::api::platform::filesystem;
        if (fs::IsDirectory(resolved)) {
            static const char* kFaces[6] = {"posx.jpg", "negx.jpg", "posy.jpg", "negy.jpg", "posz.jpg", "negz.jpg"};
            std::array<std::string, 6> facePaths;
            bool allFound = true;
            for (int i = 0; i < 6; ++i) {
                facePaths[i] = ResolveAssetPath(normalizedId + "/" + kFaces[i], assetRoots, assetRegistry);
                if (facePaths[i].empty()) {
                    eyelib::api::Log::WarnF("SkyboxCacheNew: missing face %s/%s",
                                            normalizedId.c_str(), kFaces[i]);
                    allFound = false;
                    break;
                }
            }
            if (allFound)
                loaded = entry->LoadSixFaces(device, facePaths, /*isSRGB=*/true, /*generateMipmaps=*/true);
        } else {
            loaded = entry->LoadCrossLayout(device, resolved, crossFaceSize, /*isSRGB=*/true,
                                            /*generateMipmaps=*/true);
        }
    }

    if (!loaded) {
        // Cache the failure as a "no sky" sentinel so we don't keep retrying the same
        // broken asset every frame. Falling back to procedural would mask configuration
        // mistakes; mirror the legacy behaviour of "no skybox if asset failed".
        entry->Reset();
    }

    auto* raw = entry.get();
    m_cache.emplace(normalizedId, std::move(entry));
    return raw;
}

} // namespace openeye::player
