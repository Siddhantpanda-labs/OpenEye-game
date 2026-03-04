#pragma once

#include "api/assets/AssetId.h"
#include "api/assets/AssetTypes.h"
#include "api/animation/AnimationClip.h"
#include "api/animation/Skeleton.h"
#include "api/render/Material.h"
#include "api/render/RenderDevice.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace api {

/**
 * @brief Client-facing asset registry façade.
 *
 * Does not expose JSON or internal engine metadata types.
 */
class AssetRegistry {
public:
    AssetRegistry();
    ~AssetRegistry();

    bool RegisterAsset(const AssetId& id, const std::string& filePath, const std::string& assetType);
    AssetId RegisterAsset(const std::string& filePath, const std::string& assetType);
    AssetId LoadAsset(const std::string& filePath, AssetType type);
    /** CPU-only: register and store descriptor; no GPU creation. Use from binding so GPU is created on first Get*. */
    AssetId LoadAssetDescriptorOnly(const std::string& filePath, AssetType type);
    void UnloadAsset(const AssetId& id);
    bool ReloadAsset(const AssetId& id);

    bool IsRegistered(const AssetId& id) const;
    bool IsRegistered(const std::string& filePath) const;

    AssetId GetId(const std::string& filePath) const;
    std::string GetFilePath(const AssetId& id) const;
    std::string GetAssetPath(const AssetId& id) const;
    AssetType GetAssetType(const AssetId& id) const;
    size_t GetAssetSize(const AssetId& id) const;
    size_t GetAssetMemoryUsage() const;

    std::shared_ptr<AnimationClip> GetAnimationClip(const AssetId& id, size_t index = 0) const;
    std::shared_ptr<AnimationClip> GetAnimationClip(const std::string& path, size_t index = 0) const;
    std::shared_ptr<Skeleton> GetSkeleton(const AssetId& id) const;
    std::shared_ptr<Skeleton> GetSkeleton(const std::string& path) const;
    std::shared_ptr<Material> GetMaterial(const AssetId& id) const;
    std::shared_ptr<Material> GetMaterial(const std::string& path) const;

    void AddDependency(const AssetId& asset, const AssetId& dependency);
    void RemoveDependency(const AssetId& asset, const AssetId& dependency);
    std::vector<AssetId> GetDependencies(const AssetId& id) const;
    std::vector<AssetId> GetDependents(const AssetId& id) const;

    /** Set device used when loading GPU assets (shaders, textures). Call once at app/render startup. Sets ready-for-resolution. */
    void SetRenderDevice(RenderDevice device);
    /** Device currently used for GPU asset resolution. Null if not set. */
    RenderDevice GetRenderDevice() const;
    /** True after SetRenderDevice(non-null). False after Clear or device loss. GetMesh/GetMaterial assert ready before resolving. */
    bool IsReadyForResolution() const;

    /** Device loss: clear GPU instance caches; descriptors kept. Call before destroying the device. Then: pipeline->Shutdown(), destroy device, create new device, SetRenderDevice(newDevice), pipeline->Initialize(newDevice, w, h). */
    void OnDeviceLost();

    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);

    void Clear();

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

