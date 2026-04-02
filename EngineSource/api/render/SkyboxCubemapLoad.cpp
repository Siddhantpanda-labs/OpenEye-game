#include "api/render/SkyboxCubemapLoad.h"

#include "api/render/RenderDevice.h"
#include "api/render/TextureCube.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"

namespace eyelib {
namespace render {

std::shared_ptr<api::TextureCube> LoadSkyboxCubemapFromAssetId(api::RenderDevice device, const std::string& assetId,
                                                               const SkyboxPathResolver& resolvePath, uint32_t faceSizeForCross) {
    if (!device || assetId.empty() || !resolvePath)
        return nullptr;

    const std::string resolved = resolvePath(assetId);
    if (resolved.empty())
        return nullptr;

    if (!platform::filesystem::IsDirectory(resolved)) {
        auto skybox = std::make_shared<api::TextureCube>();
        if (skybox->LoadFromCrossLayout(device, resolved, faceSizeForCross, true, true))
            return skybox;
        core::Log::WarnF("LoadSkyboxCubemapFromAssetId: cross layout failed for %s", assetId.c_str());
        return nullptr;
    }

    static const char* kFaces[] = {"posx.jpg", "negx.jpg", "posy.jpg", "negy.jpg", "posz.jpg", "negz.jpg"};
    std::string paths[6];
    for (int i = 0; i < 6; ++i) {
        paths[i] = resolvePath(assetId + "/" + kFaces[i]);
        if (paths[i].empty()) {
            core::Log::WarnF("LoadSkyboxCubemapFromAssetId: missing face %s/%s", assetId.c_str(), kFaces[i]);
            return nullptr;
        }
    }

    auto skybox = std::make_shared<api::TextureCube>();
    if (!skybox->LoadFromFiles(device, paths[0], paths[1], paths[2], paths[3], paths[4], paths[5], true, true)) {
        core::Log::WarnF("LoadSkyboxCubemapFromAssetId: LoadFromFiles failed for %s", assetId.c_str());
        return nullptr;
    }
    return skybox;
}

} // namespace render
} // namespace eyelib
