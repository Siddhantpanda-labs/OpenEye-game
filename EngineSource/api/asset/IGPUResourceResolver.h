#pragma once

#include "api/asset/AssetId.h"
#include <memory>

namespace eyelib {
namespace render {
class Shader;
class Mesh;
class Texture2D;
class Material;
}
namespace api {

/**
 * Optional resolver for GPU resource creation and caching.
 * When set on AssetRegistry, GetShader/GetMesh/GetTexture/GetMaterial delegate here.
 */
class IGPUResourceResolver {
public:
    virtual ~IGPUResourceResolver() = default;

    virtual std::shared_ptr<::eyelib::render::Shader> GetShader(const AssetId& id) = 0;
    virtual std::shared_ptr<::eyelib::render::Mesh> GetMesh(const AssetId& id) = 0;
    virtual std::shared_ptr<::eyelib::render::Texture2D> GetTexture(const AssetId& id) = 0;
    virtual std::shared_ptr<::eyelib::render::Material> GetMaterial(const AssetId& id) = 0;

    virtual void OnDeviceLost() = 0;
    /** Drop cached GPU material so the next GetMaterial rebuilds from descriptor (e.g. after editor save). */
    virtual void InvalidateMaterial(const AssetId& id) { (void)id; }
};

} // namespace api
} // namespace eyelib
