#pragma once

#include <functional>
#include <memory>
#include <string>

namespace eyelib {
namespace api {
struct RenderDevice;
class TextureCube;
} // namespace api

namespace render {

/** Resolves a skybox asset id (path or GUID string) to an absolute path or project-relative path that exists on disk. */
using SkyboxPathResolver = std::function<std::string(const std::string& id)>;

/**
 * Load a cubemap from an asset id: single-file horizontal cross (not a directory), or a directory with posx.jpg … negz.jpg.
 * \param faceSizeForCross  Per-face pixel size when using cross layout (e.g. 512 for 2048×1536 cross).
 */
std::shared_ptr<api::TextureCube> LoadSkyboxCubemapFromAssetId(api::RenderDevice device, const std::string& assetId,
                                                               const SkyboxPathResolver& resolvePath, uint32_t faceSizeForCross);

} // namespace render
} // namespace eyelib
