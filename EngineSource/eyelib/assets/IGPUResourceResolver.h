#pragma once

#include "eyelib/assets/AssetId.h"
#include <memory>

namespace eyelib {
namespace render {
class Shader;
class Mesh;
class Texture2D;
class Material;
}
namespace assets {

/**
 * Optional resolver for GPU resource creation and caching.
 * When set on AssetManager, GetShader/GetMesh/GetTexture/GetMaterial delegate here
 * so the renderer owns the GPU instance cache (design: GPU creation fully in the renderer).
 */
class IGPUResourceResolver {
public:
    virtual ~IGPUResourceResolver() = default;

    virtual std::shared_ptr<::eyelib::render::Shader> GetShader(const AssetId& id) = 0;
    virtual std::shared_ptr<::eyelib::render::Mesh> GetMesh(const AssetId& id) = 0;
    virtual std::shared_ptr<::eyelib::render::Texture2D> GetTexture(const AssetId& id) = 0;
    virtual std::shared_ptr<::eyelib::render::Material> GetMaterial(const AssetId& id) = 0;

    /** Clear GPU instance caches (e.g. on device lost). Descriptors remain in AssetManager. */
    virtual void OnDeviceLost() = 0;
};

} // namespace assets
} // namespace eyelib
