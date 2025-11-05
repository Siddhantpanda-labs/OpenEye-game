#pragma once

#include "AssetGUID.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <json.hpp>

namespace OpenEye {
namespace Assets {

/**
 * @brief Asset metadata stored in the registry.
 */
struct AssetMetadata {
    AssetGUID guid;
    std::string filePath;
    std::string assetType; // "mesh", "texture", "material", "animation", etc.
    uint32_t version = 1;
    std::vector<AssetGUID> dependencies; // Assets this asset depends on
    std::vector<AssetGUID> dependents;   // Assets that depend on this asset
    
    // Metadata
    std::string name;
    std::string author;
    std::string description;
    uint64_t fileSize = 0;
    uint64_t lastModified = 0;
    
    // Serialization
    nlohmann::json ToJson() const;
    static AssetMetadata FromJson(const nlohmann::json& json);
};

/**
 * @brief Asset Registry manages asset GUIDs, file paths, and dependencies.
 * 
 * Provides:
 * - GUID to file path mapping
 * - File path to GUID mapping
 * - Dependency graph tracking
 * - Asset metadata storage
 * - Cache versioning
 */
class AssetRegistry {
public:
    /**
     * @brief Register an asset with a GUID.
     * @param guid Asset GUID
     * @param filePath File path to the asset
     * @param assetType Type of asset ("mesh", "texture", "material", etc.)
     * @return true if successful, false if GUID already exists
     */
    bool RegisterAsset(const AssetGUID& guid, const std::string& filePath, const std::string& assetType);
    
    /**
     * @brief Register an asset and generate a new GUID.
     * @param filePath File path to the asset
     * @param assetType Type of asset
     * @return Generated GUID, or Invalid if failed
     */
    AssetGUID RegisterAsset(const std::string& filePath, const std::string& assetType);
    
    /**
     * @brief Get asset metadata by GUID.
     * @param guid Asset GUID
     * @return Pointer to metadata, or nullptr if not found
     */
    AssetMetadata* GetMetadata(const AssetGUID& guid);
    const AssetMetadata* GetMetadata(const AssetGUID& guid) const;
    
    /**
     * @brief Get asset GUID by file path.
     * @param filePath File path
     * @return Asset GUID, or Invalid if not found
     */
    AssetGUID GetGUID(const std::string& filePath) const;
    
    /**
     * @brief Get file path by GUID.
     * @param guid Asset GUID
     * @return File path, or empty string if not found
     */
    std::string GetFilePath(const AssetGUID& guid) const;
    
    /**
     * @brief Add a dependency between two assets.
     * @param asset GUID of asset that depends on another
     * @param dependency GUID of asset being depended upon
     */
    void AddDependency(const AssetGUID& asset, const AssetGUID& dependency);
    
    /**
     * @brief Remove a dependency.
     * @param asset GUID of asset
     * @param dependency GUID of dependency to remove
     */
    void RemoveDependency(const AssetGUID& asset, const AssetGUID& dependency);
    
    /**
     * @brief Get all dependencies of an asset.
     * @param guid Asset GUID
     * @return Vector of dependency GUIDs
     */
    std::vector<AssetGUID> GetDependencies(const AssetGUID& guid) const;
    
    /**
     * @brief Get all assets that depend on this asset.
     * @param guid Asset GUID
     * @return Vector of dependent asset GUIDs
     */
    std::vector<AssetGUID> GetDependents(const AssetGUID& guid) const;
    
    /**
     * @brief Check if asset is registered.
     * @param guid Asset GUID
     * @return true if registered, false otherwise
     */
    bool IsRegistered(const AssetGUID& guid) const;
    
    /**
     * @brief Check if file path is registered.
     * @param filePath File path
     * @return true if registered, false otherwise
     */
    bool IsRegistered(const std::string& filePath) const;
    
    /**
     * @brief Unregister an asset.
     * @param guid Asset GUID
     */
    void UnregisterAsset(const AssetGUID& guid);
    
    /**
     * @brief Update asset version (for cache invalidation).
     * @param guid Asset GUID
     * @param newVersion New version number
     */
    void UpdateVersion(const AssetGUID& guid, uint32_t newVersion);
    
    /**
     * @brief Save registry to JSON file.
     * @param filepath Path to save file
     * @return true if successful, false otherwise
     */
    bool SaveToFile(const std::string& filepath) const;
    
    /**
     * @brief Load registry from JSON file.
     * @param filepath Path to load file from
     * @return true if successful, false otherwise
     */
    bool LoadFromFile(const std::string& filepath);
    
    /**
     * @brief Get all registered asset GUIDs.
     * @return Vector of all GUIDs
     */
    std::vector<AssetGUID> GetAllGUIDs() const;
    
    /**
     * @brief Clear all registered assets.
     */
    void Clear();

private:
    // GUID -> Metadata mapping
    std::unordered_map<AssetGUID, AssetMetadata, AssetGUID::Hash> m_assets;
    
    // File path -> GUID mapping
    std::unordered_map<std::string, AssetGUID> m_pathToGUID;
};

} // namespace Assets
} // namespace OpenEye

