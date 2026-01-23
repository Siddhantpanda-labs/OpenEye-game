#pragma once

#include "api/assets/AssetId.h"

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

    bool IsRegistered(const AssetId& id) const;
    bool IsRegistered(const std::string& filePath) const;

    AssetId GetId(const std::string& filePath) const;
    std::string GetFilePath(const AssetId& id) const;

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

