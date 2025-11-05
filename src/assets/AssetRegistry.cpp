#include "AssetRegistry.h"
#include "core/log/Log.h"
#include <fstream>
#include <algorithm>

namespace OpenEye {
namespace Assets {

// ============================================================================
// AssetMetadata Implementation
// ============================================================================

nlohmann::json AssetMetadata::ToJson() const {
    nlohmann::json json;
    json["guid"] = guid.ToString();
    json["filePath"] = filePath;
    json["assetType"] = assetType;
    json["version"] = version;
    json["name"] = name;
    json["author"] = author;
    json["description"] = description;
    json["fileSize"] = fileSize;
    json["lastModified"] = lastModified;
    
    // Serialize dependencies
    json["dependencies"] = nlohmann::json::array();
    for (const auto& dep : dependencies) {
        json["dependencies"].push_back(dep.ToString());
    }
    
    return json;
}

AssetMetadata AssetMetadata::FromJson(const nlohmann::json& json) {
    AssetMetadata metadata;
    
    if (json.contains("guid")) {
        metadata.guid = AssetGUID::FromString(json["guid"]);
    }
    if (json.contains("filePath")) {
        metadata.filePath = json["filePath"];
    }
    if (json.contains("assetType")) {
        metadata.assetType = json["assetType"];
    }
    if (json.contains("version")) {
        metadata.version = json["version"];
    }
    if (json.contains("name")) {
        metadata.name = json["name"];
    }
    if (json.contains("author")) {
        metadata.author = json["author"];
    }
    if (json.contains("description")) {
        metadata.description = json["description"];
    }
    if (json.contains("fileSize")) {
        metadata.fileSize = json["fileSize"];
    }
    if (json.contains("lastModified")) {
        metadata.lastModified = json["lastModified"];
    }
    
    // Deserialize dependencies
    if (json.contains("dependencies") && json["dependencies"].is_array()) {
        for (const auto& depJson : json["dependencies"]) {
            if (depJson.is_string()) {
                metadata.dependencies.push_back(AssetGUID::FromString(depJson));
            }
        }
    }
    
    return metadata;
}

// ============================================================================
// AssetRegistry Implementation
// ============================================================================

bool AssetRegistry::RegisterAsset(const AssetGUID& guid, const std::string& filePath, const std::string& assetType) {
    if (m_assets.find(guid) != m_assets.end()) {
        OE_WARN_F("AssetRegistry: GUID already registered: %s", guid.ToString().c_str());
        return false;
    }
    
    if (m_pathToGUID.find(filePath) != m_pathToGUID.end()) {
        OE_WARN_F("AssetRegistry: File path already registered: %s", filePath.c_str());
        return false;
    }
    
    AssetMetadata metadata;
    metadata.guid = guid;
    metadata.filePath = filePath;
    metadata.assetType = assetType;
    metadata.name = filePath; // Default name is file path
    
    m_assets[guid] = metadata;
    m_pathToGUID[filePath] = guid;
    
    OE_INFO_F("AssetRegistry: Registered asset '%s' with GUID %s", filePath.c_str(), guid.ToString().c_str());
    return true;
}

AssetGUID AssetRegistry::RegisterAsset(const std::string& filePath, const std::string& assetType) {
    // Check if already registered
    auto it = m_pathToGUID.find(filePath);
    if (it != m_pathToGUID.end()) {
        return it->second;
    }
    
    // Generate new GUID
    AssetGUID guid = AssetGUID::Generate();
    
    if (RegisterAsset(guid, filePath, assetType)) {
        return guid;
    }
    
    return AssetGUID::Invalid();
}

AssetMetadata* AssetRegistry::GetMetadata(const AssetGUID& guid) {
    auto it = m_assets.find(guid);
    if (it != m_assets.end()) {
        return &it->second;
    }
    return nullptr;
}

const AssetMetadata* AssetRegistry::GetMetadata(const AssetGUID& guid) const {
    auto it = m_assets.find(guid);
    if (it != m_assets.end()) {
        return &it->second;
    }
    return nullptr;
}

AssetGUID AssetRegistry::GetGUID(const std::string& filePath) const {
    auto it = m_pathToGUID.find(filePath);
    if (it != m_pathToGUID.end()) {
        return it->second;
    }
    return AssetGUID::Invalid();
}

std::string AssetRegistry::GetFilePath(const AssetGUID& guid) const {
    auto it = m_assets.find(guid);
    if (it != m_assets.end()) {
        return it->second.filePath;
    }
    return "";
}

void AssetRegistry::AddDependency(const AssetGUID& asset, const AssetGUID& dependency) {
    auto* assetMeta = GetMetadata(asset);
    auto* depMeta = GetMetadata(dependency);
    
    if (!assetMeta || !depMeta) {
        OE_WARN("AssetRegistry: Cannot add dependency - asset or dependency not found");
        return;
    }
    
    // Add to asset's dependencies (if not already present)
    if (std::find(assetMeta->dependencies.begin(), assetMeta->dependencies.end(), dependency) == assetMeta->dependencies.end()) {
        assetMeta->dependencies.push_back(dependency);
    }
    
    // Add to dependency's dependents (if not already present)
    if (std::find(depMeta->dependents.begin(), depMeta->dependents.end(), asset) == depMeta->dependents.end()) {
        depMeta->dependents.push_back(asset);
    }
}

void AssetRegistry::RemoveDependency(const AssetGUID& asset, const AssetGUID& dependency) {
    auto* assetMeta = GetMetadata(asset);
    auto* depMeta = GetMetadata(dependency);
    
    if (!assetMeta || !depMeta) {
        return;
    }
    
    // Remove from asset's dependencies
    assetMeta->dependencies.erase(
        std::remove(assetMeta->dependencies.begin(), assetMeta->dependencies.end(), dependency),
        assetMeta->dependencies.end()
    );
    
    // Remove from dependency's dependents
    depMeta->dependents.erase(
        std::remove(depMeta->dependents.begin(), depMeta->dependents.end(), asset),
        depMeta->dependents.end()
    );
}

std::vector<AssetGUID> AssetRegistry::GetDependencies(const AssetGUID& guid) const {
    const auto* metadata = GetMetadata(guid);
    if (metadata) {
        return metadata->dependencies;
    }
    return {};
}

std::vector<AssetGUID> AssetRegistry::GetDependents(const AssetGUID& guid) const {
    const auto* metadata = GetMetadata(guid);
    if (metadata) {
        return metadata->dependents;
    }
    return {};
}

bool AssetRegistry::IsRegistered(const AssetGUID& guid) const {
    return m_assets.find(guid) != m_assets.end();
}

bool AssetRegistry::IsRegistered(const std::string& filePath) const {
    return m_pathToGUID.find(filePath) != m_pathToGUID.end();
}

void AssetRegistry::UnregisterAsset(const AssetGUID& guid) {
    auto it = m_assets.find(guid);
    if (it == m_assets.end()) {
        return;
    }
    
    // Remove from path mapping
    m_pathToGUID.erase(it->second.filePath);
    
    // Remove all dependencies
    for (const auto& dep : it->second.dependencies) {
        RemoveDependency(guid, dep);
    }
    
    // Remove from dependents
    for (const auto& dependent : it->second.dependents) {
        auto* depMeta = GetMetadata(dependent);
        if (depMeta) {
            depMeta->dependencies.erase(
                std::remove(depMeta->dependencies.begin(), depMeta->dependencies.end(), guid),
                depMeta->dependencies.end()
            );
        }
    }
    
    // Remove from registry
    m_assets.erase(it);
}

void AssetRegistry::UpdateVersion(const AssetGUID& guid, uint32_t newVersion) {
    auto* metadata = GetMetadata(guid);
    if (metadata) {
        metadata->version = newVersion;
        OE_INFO_F("AssetRegistry: Updated version of asset %s to %u", guid.ToString().c_str(), newVersion);
    }
}

bool AssetRegistry::SaveToFile(const std::string& filepath) const {
    try {
        nlohmann::json json;
        json["version"] = 1;
        json["assets"] = nlohmann::json::array();
        
        for (const auto& [guid, metadata] : m_assets) {
            json["assets"].push_back(metadata.ToJson());
        }
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            OE_ERROR_F("AssetRegistry: Failed to open file for writing: %s", filepath.c_str());
            return false;
        }
        
        file << json.dump(2); // Pretty print
        file.close();
        
        OE_INFO_F("AssetRegistry: Successfully saved registry to %s", filepath.c_str());
        return true;
    } catch (const std::exception& e) {
        OE_ERROR_F("AssetRegistry: Exception during save: %s", e.what());
        return false;
    }
}

bool AssetRegistry::LoadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            OE_ERROR_F("AssetRegistry: Failed to open file: %s", filepath.c_str());
            return false;
        }
        
        nlohmann::json json;
        file >> json;
        file.close();
        
        if (!json.contains("assets") || !json["assets"].is_array()) {
            OE_ERROR("AssetRegistry: Invalid registry file format");
            return false;
        }
        
        // Clear existing registry
        Clear();
        
        // Load assets
        for (const auto& assetJson : json["assets"]) {
            AssetMetadata metadata = AssetMetadata::FromJson(assetJson);
            if (metadata.guid.IsValid()) {
                m_assets[metadata.guid] = metadata;
                m_pathToGUID[metadata.filePath] = metadata.guid;
            }
        }
        
        // Rebuild dependency relationships
        for (auto& [guid, metadata] : m_assets) {
            for (const auto& depGuid : metadata.dependencies) {
                auto* depMeta = GetMetadata(depGuid);
                if (depMeta) {
                    if (std::find(depMeta->dependents.begin(), depMeta->dependents.end(), guid) == depMeta->dependents.end()) {
                        depMeta->dependents.push_back(guid);
                    }
                }
            }
        }
        
        OE_INFO_F("AssetRegistry: Successfully loaded registry from %s (%zu assets)", filepath.c_str(), m_assets.size());
        return true;
    } catch (const std::exception& e) {
        OE_ERROR_F("AssetRegistry: Exception during load: %s", e.what());
        return false;
    }
}

std::vector<AssetGUID> AssetRegistry::GetAllGUIDs() const {
    std::vector<AssetGUID> guids;
    guids.reserve(m_assets.size());
    for (const auto& [guid, metadata] : m_assets) {
        guids.push_back(guid);
    }
    return guids;
}

void AssetRegistry::Clear() {
    m_assets.clear();
    m_pathToGUID.clear();
    OE_INFO("AssetRegistry: Cleared all assets");
}

} // namespace Assets
} // namespace OpenEye

