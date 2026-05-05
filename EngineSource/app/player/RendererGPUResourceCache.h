#pragma once

#include "api/asset/AssetId.h"
#include "api/asset/AssetRegistry.h"
#include "api/asset/IGPUResourceResolver.h"
#include <memory>
#include <unordered_map>

namespace openeye::player {

/** Hash for api::AssetId for use in maps. */
struct ApiAssetIdHash {
    size_t operator()(const eyelib::api::AssetId& id) const {
        return id.high ^ (id.low << 1);
    }
};

/**
 * Renderer-owned GPU resource cache implementing api::IGPUResourceResolver.
 * Gets descriptors from api::AssetRegistry and creates GPU instances on miss.
 */
class RendererGPUResourceCache : public eyelib::api::IGPUResourceResolver {
public:
    explicit RendererGPUResourceCache(void* device, eyelib::api::AssetRegistry* registry);
    ~RendererGPUResourceCache() override;

    std::shared_ptr<eyelib::render::Shader> GetShader(const eyelib::api::AssetId& id) override;
    std::shared_ptr<eyelib::render::Mesh> GetMesh(const eyelib::api::AssetId& id) override;
    std::shared_ptr<eyelib::render::Texture2D> GetTexture(const eyelib::api::AssetId& id) override;
    std::shared_ptr<eyelib::render::Material> GetMaterial(const eyelib::api::AssetId& id) override;
    void OnDeviceLost() override;
    void InvalidateMaterial(const eyelib::api::AssetId& id) override;

private:
    void* m_device = nullptr;
    eyelib::api::AssetRegistry* m_registry = nullptr;
    std::unordered_map<eyelib::api::AssetId, std::shared_ptr<eyelib::render::Shader>, ApiAssetIdHash> m_shaders;
    std::unordered_map<eyelib::api::AssetId, std::shared_ptr<eyelib::render::Mesh>, ApiAssetIdHash> m_meshes;
    std::unordered_map<eyelib::api::AssetId, std::shared_ptr<eyelib::render::Texture2D>, ApiAssetIdHash> m_textures;
    std::unordered_map<eyelib::api::AssetId, std::shared_ptr<eyelib::render::Material>, ApiAssetIdHash> m_materials;
};

} // namespace openeye::player
