#pragma once

#include "eyelib/assets/AssetRegistry.h"
#include "eyelib/assets/AssetId.h"
#include "eyelib/assets/IGPUResourceResolver.h"
#include "eyelib/render/ShaderDescriptor.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/TextureDescriptor.h"
#include "eyelib/render/MaterialDescriptor.h"
#include <memory>

namespace eyelib {
namespace render {
class Shader;
class Texture2D;
}
namespace assets {

class AssetManager {
public:
    static AssetManager& Get();

    AssetRegistry& Registry() { return m_registry; }
    AssetCache& Cache() { return m_registry.Cache(); }

    /** Device used when loading GPU assets (shaders, textures, etc.). Set once at app/render startup; never switch.
     *  Lifetime rule: registry is longer-lived (e.g. Editor); only resources created with this device may be cached.
     *  Sets ready-for-resolution so Get* can create from descriptors. Clear/OnDeviceLost clear ready. */
    void SetRenderDevice(void* device);
    void* GetRenderDevice() const { return m_renderDevice; }
    /** True after SetRenderDevice(non-null). False after Clear or OnDeviceLost. Get* asserts this before resolving. */
    bool IsReadyForResolution() const { return m_readyForResolution; }

    /** Phase 3c: Store CPU descriptor and register GPU instance when loading. Loaders call these after Cache().Store(...). */
    void StoreShaderDescriptor(const AssetId& id, const eyelib::render::ShaderDescriptor& desc);
    void StoreMeshDescriptor(const AssetId& id, const eyelib::render::MeshDescriptor& desc);
    void StoreTextureDescriptor(const AssetId& id, const eyelib::render::TextureDescriptor& desc);
    void StoreMaterialDescriptor(const AssetId& id, const eyelib::render::MaterialDescriptor& desc);
    void RegisterShaderInstance(const AssetId& id, void* device, std::shared_ptr<eyelib::render::Shader> instance);
    void RegisterMeshInstance(const AssetId& id, void* device, std::shared_ptr<eyelib::render::Mesh> instance);
    void RegisterTextureInstance(const AssetId& id, void* device, std::shared_ptr<eyelib::render::Texture2D> instance);
    void RegisterMaterialInstance(const AssetId& id, void* device, std::shared_ptr<eyelib::render::Material> instance);

    /** Descriptor-only lookup (no GPU creation). Used by renderer-owned resolver to create instances. */
    const eyelib::render::ShaderDescriptor* GetShaderDescriptor(const AssetId& id) const;
    const eyelib::render::MeshDescriptor* GetMeshDescriptor(const AssetId& id) const;
    const eyelib::render::TextureDescriptor* GetTextureDescriptor(const AssetId& id) const;
    const eyelib::render::MaterialDescriptor* GetMaterialDescriptor(const AssetId& id) const;

    /** When set, GetShader/GetMesh/GetTexture/GetMaterial delegate to the resolver (renderer-owned cache). */
    void SetGPUResolver(IGPUResourceResolver* resolver);
    IGPUResourceResolver* GetGPUResolver() const { return m_resolver; }

    /** Phase 3c: Resolve to GPU instance for current device (descriptor -> instance, or legacy cache). */
    std::shared_ptr<eyelib::render::Shader> GetShader(const AssetId& id);
    std::shared_ptr<eyelib::render::Mesh> GetMesh(const AssetId& id);
    std::shared_ptr<eyelib::render::Texture2D> GetTexture(const AssetId& id);
    std::shared_ptr<eyelib::render::Material> GetMaterial(const AssetId& id);

    /** Clear descriptor and instance caches (call when registry/cache is cleared). */
    void ClearDescriptorsAndInstances();

    /** Device loss (§9): Clear per-device GPU instance caches only. Descriptors are kept so that after
     *  creating a new device and calling SetRenderDevice(newDevice), Get* will re-create from descriptors.
     *  Call before destroying the old device. App must then: pipeline->Shutdown(), destroy device,
     *  create new device, SetRenderDevice(newDevice), pipeline->Initialize(newDevice, w, h). */
    void OnDeviceLost();

private:
    AssetManager();
    ~AssetManager();
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    AssetRegistry m_registry;
    void* m_renderDevice = nullptr;
    bool m_readyForResolution = false;
    IGPUResourceResolver* m_resolver = nullptr;
};

} // namespace assets
} // namespace eyelib
