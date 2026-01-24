#pragma once

#include "api/assets/AssetId.h"
#include "api/assets/AssetTypes.h"

#include <cstddef>
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

    void AddDependency(const AssetId& asset, const AssetId& dependency);
    void RemoveDependency(const AssetId& asset, const AssetId& dependency);
    std::vector<AssetId> GetDependencies(const AssetId& id) const;
    std::vector<AssetId> GetDependents(const AssetId& id) const;

    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);

    void Clear();

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

