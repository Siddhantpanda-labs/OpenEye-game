#pragma once

#include "eyelib/assets/AssetId.h"
#include "eyelib/assets/AssetTypes.h"
#include "eyelib/assets/AssetCache.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace assets {

struct AssetMetadata {
    AssetId id{};
    std::string filePath{};
    std::string assetType{};
    uint32_t version = 1;

    std::vector<AssetId> dependencies{};
    std::vector<AssetId> dependents{};

    std::string name{};
    std::string author{};
    std::string description{};
    uint64_t fileSize = 0;
    uint64_t lastModified = 0;
};

class AssetRegistry {
public:
    bool RegisterAsset(const AssetId& id, const std::string& filePath, const std::string& assetType);
    AssetId RegisterAsset(const std::string& filePath, const std::string& assetType);
    AssetId LoadAsset(const std::string& filePath, AssetType type);
    AssetId LoadAsset(const AssetId& id);
    /** CPU-only: register and store descriptor; no GPU creation. Use from binding phase so GPU is created on first Get*. */
    AssetId LoadAssetDescriptorOnly(const std::string& filePath, AssetType type);
    AssetId LoadAssetDescriptorOnly(const AssetId& id);
    void UnloadAsset(const AssetId& id);
    bool ReloadAsset(const AssetId& id);

    template<typename T>
    T* GetAsset(const AssetId& id) {
        return m_cache.Get<T>(id).get();
    }

    AssetMetadata* GetMetadata(const AssetId& id);
    const AssetMetadata* GetMetadata(const AssetId& id) const;

    AssetId GetId(const std::string& filePath) const;
    std::string GetFilePath(const AssetId& id) const;
    std::string GetAssetPath(const AssetId& id) const { return GetFilePath(id); }
    AssetType GetAssetType(const AssetId& id) const;
    size_t GetAssetSize(const AssetId& id) const;
    size_t GetAssetMemoryUsage() const;
    AssetCache& Cache() { return m_cache; }
    const AssetCache& Cache() const { return m_cache; }

    void AddDependency(const AssetId& asset, const AssetId& dependency);
    void RemoveDependency(const AssetId& asset, const AssetId& dependency);

    std::vector<AssetId> GetDependencies(const AssetId& id) const;
    std::vector<AssetId> GetDependents(const AssetId& id) const;

    bool IsRegistered(const AssetId& id) const;
    bool IsRegistered(const std::string& filePath) const;

    void UnregisterAsset(const AssetId& id);
    void UpdateVersion(const AssetId& id, uint32_t newVersion);

    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);

    std::vector<AssetId> GetAllIds() const;
    void Clear();

private:
    std::unordered_map<AssetId, AssetMetadata, AssetId::Hash> m_assets;
    std::unordered_map<std::string, AssetId> m_pathToId;
    std::unordered_map<AssetId, uint32_t, AssetId::Hash> m_refCounts;
    AssetCache m_cache;
    size_t m_memoryUsage = 0;
};

} // namespace assets
} // namespace eyelib

