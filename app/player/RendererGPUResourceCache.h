#pragma once

#include "eyelib/assets/IGPUResourceResolver.h"
#include "eyelib/assets/AssetId.h"
#include <memory>
#include <unordered_map>

namespace eyelib {
namespace assets {
class AssetManager;
}
namespace render {
class Shader;
class Mesh;
class Texture2D;
class Material;
}
}

namespace openeye::player {

/**
 * Renderer-owned GPU resource cache implementing IGPUResourceResolver.
 * Holds device and instance caches; creates from AssetManager descriptors on miss.
 * Set on AssetManager so Get* delegates here (GPU creation fully in the renderer).
 */
class RendererGPUResourceCache : public eyelib::assets::IGPUResourceResolver {
public:
    explicit RendererGPUResourceCache(void* device, eyelib::assets::AssetManager* manager);
    ~RendererGPUResourceCache() override;

    std::shared_ptr<eyelib::render::Shader> GetShader(const eyelib::assets::AssetId& id) override;
    std::shared_ptr<eyelib::render::Mesh> GetMesh(const eyelib::assets::AssetId& id) override;
    std::shared_ptr<eyelib::render::Texture2D> GetTexture(const eyelib::assets::AssetId& id) override;
    std::shared_ptr<eyelib::render::Material> GetMaterial(const eyelib::assets::AssetId& id) override;
    void OnDeviceLost() override;

private:
    void* m_device = nullptr;
    eyelib::assets::AssetManager* m_manager = nullptr;
    std::unordered_map<eyelib::assets::AssetId, std::shared_ptr<eyelib::render::Shader>, eyelib::assets::AssetId::Hash> m_shaders;
    std::unordered_map<eyelib::assets::AssetId, std::shared_ptr<eyelib::render::Mesh>, eyelib::assets::AssetId::Hash> m_meshes;
    std::unordered_map<eyelib::assets::AssetId, std::shared_ptr<eyelib::render::Texture2D>, eyelib::assets::AssetId::Hash> m_textures;
    std::unordered_map<eyelib::assets::AssetId, std::shared_ptr<eyelib::render::Material>, eyelib::assets::AssetId::Hash> m_materials;
};

} // namespace openeye::player
