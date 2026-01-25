#include "eyelib/assets/AssetRegistry.h"

#include "eyelib/core/Log.h"
#include "eyelib/assets/AssetManager.h"
#include "eyelib/assets/loaders/AnimationLoader.h"
#include "eyelib/assets/loaders/AudioLoader.h"
#include "eyelib/assets/loaders/MaterialLoader.h"
#include "eyelib/assets/loaders/MeshLoader.h"
#include "eyelib/assets/loaders/ShaderLoader.h"
#include "eyelib/assets/loaders/TextureLoader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <json.hpp>

namespace eyelib {
namespace assets {

namespace {
static void FillFileMetadata(const std::string& filePath, AssetMetadata& md) {
    try {
        std::filesystem::path p(filePath);
        if (std::filesystem::exists(p)) {
            md.fileSize = static_cast<uint64_t>(std::filesystem::file_size(p));
            auto t = std::filesystem::last_write_time(p).time_since_epoch().count();
            md.lastModified = static_cast<uint64_t>(t);
        }
    } catch (...) {
    }
}
} // namespace

static nlohmann::json ToJson(const AssetMetadata& md) {
    nlohmann::json json;
    json["id"] = md.id.ToString();
    json["filePath"] = md.filePath;
    json["assetType"] = md.assetType;
    json["version"] = md.version;
    json["name"] = md.name;
    json["author"] = md.author;
    json["description"] = md.description;
    json["fileSize"] = md.fileSize;
    json["lastModified"] = md.lastModified;

    json["dependencies"] = nlohmann::json::array();
    for (const auto& dep : md.dependencies) {
        json["dependencies"].push_back(dep.ToString());
    }

    return json;
}

static AssetMetadata FromJson(const nlohmann::json& json) {
    AssetMetadata md{};

    if (json.contains("id")) md.id = AssetId::FromString(json["id"]);
    if (json.contains("filePath")) md.filePath = json["filePath"];
    if (json.contains("assetType")) md.assetType = json["assetType"];
    if (json.contains("version")) md.version = json["version"];
    if (json.contains("name")) md.name = json["name"];
    if (json.contains("author")) md.author = json["author"];
    if (json.contains("description")) md.description = json["description"];
    if (json.contains("fileSize")) md.fileSize = json["fileSize"];
    if (json.contains("lastModified")) md.lastModified = json["lastModified"];

    if (json.contains("dependencies") && json["dependencies"].is_array()) {
        for (const auto& depJson : json["dependencies"]) {
            if (depJson.is_string()) {
                md.dependencies.push_back(AssetId::FromString(depJson));
            }
        }
    }

    return md;
}

bool AssetRegistry::RegisterAsset(const AssetId& id, const std::string& filePath, const std::string& assetType) {
    if (m_assets.find(id) != m_assets.end()) {
        core::Log::WarnF("AssetRegistry: id already registered: %s", id.ToString().c_str());
        return false;
    }

    if (m_pathToId.find(filePath) != m_pathToId.end()) {
        core::Log::WarnF("AssetRegistry: path already registered: %s", filePath.c_str());
        return false;
    }

    AssetMetadata md;
    md.id = id;
    md.filePath = filePath;
    md.assetType = assetType;
    md.name = filePath;
    FillFileMetadata(filePath, md);

    m_assets[id] = md;
    m_pathToId[filePath] = id;

    core::Log::InfoF("AssetRegistry: registered '%s' id=%s", filePath.c_str(), id.ToString().c_str());
    return true;
}

AssetId AssetRegistry::RegisterAsset(const std::string& filePath, const std::string& assetType) {
    auto it = m_pathToId.find(filePath);
    if (it != m_pathToId.end()) {
        return it->second;
    }

    AssetId id = AssetId::Generate();
    if (RegisterAsset(id, filePath, assetType)) {
        return id;
    }
    return AssetId::Invalid();
}

AssetId AssetRegistry::LoadAsset(const std::string& filePath, AssetType type) {
    const std::string typeStr = ToString(type);
    const AssetId id = RegisterAsset(filePath, typeStr);
    if (!id.IsValid()) return id;

    auto it = m_refCounts.find(id);
    if (it != m_refCounts.end()) {
        it->second += 1;
        return id;
    }
    m_refCounts[id] = 1;

    switch (type) {
        case AssetType::Mesh: MeshLoader::LoadMesh(filePath); break;
        case AssetType::Texture: TextureLoader::LoadTexture(filePath, true); break;
        case AssetType::Shader: {
            const std::string lower = filePath;
            if (lower.rfind(".vert") != std::string::npos) {
                const std::string frag = filePath.substr(0, filePath.size() - 5) + ".frag";
                ShaderLoader::LoadShader(filePath, frag);
            } else if (lower.rfind(".frag") != std::string::npos) {
                const std::string vert = filePath.substr(0, filePath.size() - 5) + ".vert";
                ShaderLoader::LoadShader(vert, filePath);
            } else {
                ShaderLoader::LoadShader(filePath, filePath);
            }
            break;
        }
        case AssetType::Audio: AudioLoader::LoadAudio(filePath); break;
        case AssetType::Animation: AnimationLoader::LoadAnimation(filePath); break;
        case AssetType::Material: MaterialLoader::LoadMaterial(filePath); break;
        default: break;
    }

    m_memoryUsage += GetAssetSize(id);
    return id;
}

AssetId AssetRegistry::LoadAsset(const AssetId& id) {
    const auto* md = GetMetadata(id);
    if (!md) return AssetId::Invalid();
    const auto type = AssetTypeFromString(md->assetType);
    return LoadAsset(md->filePath, type);
}

void AssetRegistry::UnloadAsset(const AssetId& id) {
    auto it = m_refCounts.find(id);
    if (it == m_refCounts.end()) return;
    if (it->second > 1) {
        it->second -= 1;
        return;
    }
    m_refCounts.erase(it);
    const size_t size = GetAssetSize(id);
    m_memoryUsage = (m_memoryUsage > size) ? (m_memoryUsage - size) : 0;
    m_cache.Remove(id);
}

bool AssetRegistry::ReloadAsset(const AssetId& id) {
    const auto* md = GetMetadata(id);
    if (!md) return false;
    return LoadAsset(md->filePath, AssetTypeFromString(md->assetType)).IsValid();
}

AssetMetadata* AssetRegistry::GetMetadata(const AssetId& id) {
    auto it = m_assets.find(id);
    return (it != m_assets.end()) ? &it->second : nullptr;
}

const AssetMetadata* AssetRegistry::GetMetadata(const AssetId& id) const {
    auto it = m_assets.find(id);
    return (it != m_assets.end()) ? &it->second : nullptr;
}

AssetId AssetRegistry::GetId(const std::string& filePath) const {
    auto it = m_pathToId.find(filePath);
    return (it != m_pathToId.end()) ? it->second : AssetId::Invalid();
}

std::string AssetRegistry::GetFilePath(const AssetId& id) const {
    auto it = m_assets.find(id);
    return (it != m_assets.end()) ? it->second.filePath : std::string{};
}

AssetType AssetRegistry::GetAssetType(const AssetId& id) const {
    const auto* md = GetMetadata(id);
    return md ? AssetTypeFromString(md->assetType) : AssetType::Unknown;
}

size_t AssetRegistry::GetAssetSize(const AssetId& id) const {
    const auto* md = GetMetadata(id);
    return md ? static_cast<size_t>(md->fileSize) : 0;
}

size_t AssetRegistry::GetAssetMemoryUsage() const {
    return m_memoryUsage;
}

void AssetRegistry::AddDependency(const AssetId& asset, const AssetId& dependency) {
    auto* assetMd = GetMetadata(asset);
    auto* depMd = GetMetadata(dependency);
    if (!assetMd || !depMd) {
        core::Log::Warn("AssetRegistry: cannot add dependency (missing asset/dep)");
        return;
    }

    if (std::find(assetMd->dependencies.begin(), assetMd->dependencies.end(), dependency) == assetMd->dependencies.end()) {
        assetMd->dependencies.push_back(dependency);
    }
    if (std::find(depMd->dependents.begin(), depMd->dependents.end(), asset) == depMd->dependents.end()) {
        depMd->dependents.push_back(asset);
    }
}

void AssetRegistry::RemoveDependency(const AssetId& asset, const AssetId& dependency) {
    auto* assetMd = GetMetadata(asset);
    auto* depMd = GetMetadata(dependency);
    if (!assetMd || !depMd) return;

    assetMd->dependencies.erase(
        std::remove(assetMd->dependencies.begin(), assetMd->dependencies.end(), dependency),
        assetMd->dependencies.end()
    );

    depMd->dependents.erase(
        std::remove(depMd->dependents.begin(), depMd->dependents.end(), asset),
        depMd->dependents.end()
    );
}

std::vector<AssetId> AssetRegistry::GetDependencies(const AssetId& id) const {
    const auto* md = GetMetadata(id);
    return md ? md->dependencies : std::vector<AssetId>{};
}

std::vector<AssetId> AssetRegistry::GetDependents(const AssetId& id) const {
    const auto* md = GetMetadata(id);
    return md ? md->dependents : std::vector<AssetId>{};
}

bool AssetRegistry::IsRegistered(const AssetId& id) const {
    return m_assets.find(id) != m_assets.end();
}

bool AssetRegistry::IsRegistered(const std::string& filePath) const {
    return m_pathToId.find(filePath) != m_pathToId.end();
}

void AssetRegistry::UnregisterAsset(const AssetId& id) {
    auto it = m_assets.find(id);
    if (it == m_assets.end()) return;

    m_pathToId.erase(it->second.filePath);

    for (const auto& dep : it->second.dependencies) {
        RemoveDependency(id, dep);
    }

    for (const auto& dependent : it->second.dependents) {
        auto* depMd = GetMetadata(dependent);
        if (depMd) {
            depMd->dependencies.erase(
                std::remove(depMd->dependencies.begin(), depMd->dependencies.end(), id),
                depMd->dependencies.end()
            );
        }
    }

    m_assets.erase(it);
    m_refCounts.erase(id);
    m_cache.Remove(id);
}

void AssetRegistry::UpdateVersion(const AssetId& id, uint32_t newVersion) {
    auto* md = GetMetadata(id);
    if (!md) return;
    md->version = newVersion;
    core::Log::InfoF("AssetRegistry: updated version id=%s to %u", id.ToString().c_str(), newVersion);
}

bool AssetRegistry::SaveToFile(const std::string& filepath) const {
    try {
        nlohmann::json json;
        json["version"] = 1;
        json["assets"] = nlohmann::json::array();

        for (const auto& [id, md] : m_assets) {
            (void)id;
            json["assets"].push_back(ToJson(md));
        }

        std::ofstream file(filepath);
        if (!file.is_open()) {
            core::Log::ErrorF("AssetRegistry: failed to open for write: %s", filepath.c_str());
            return false;
        }

        file << json.dump(2);
        return true;
    } catch (const std::exception& e) {
        core::Log::ErrorF("AssetRegistry: save exception: %s", e.what());
        return false;
    }
}

bool AssetRegistry::LoadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            core::Log::ErrorF("AssetRegistry: failed to open: %s", filepath.c_str());
            return false;
        }

        nlohmann::json json;
        file >> json;

        if (!json.contains("assets") || !json["assets"].is_array()) {
            core::Log::Error("AssetRegistry: invalid format (missing assets array)");
            return false;
        }

        Clear();

        for (const auto& assetJson : json["assets"]) {
            AssetMetadata md = FromJson(assetJson);
            if (md.id.IsValid()) {
                m_assets[md.id] = md;
                m_pathToId[md.filePath] = md.id;
            }
        }

        // rebuild dependents
        for (auto& [id, md] : m_assets) {
            for (const auto& depId : md.dependencies) {
                auto* depMd = GetMetadata(depId);
                if (!depMd) continue;
                if (std::find(depMd->dependents.begin(), depMd->dependents.end(), id) == depMd->dependents.end()) {
                    depMd->dependents.push_back(id);
                }
            }
        }

        core::Log::InfoF("AssetRegistry: loaded %zu assets from %s", m_assets.size(), filepath.c_str());
        return true;
    } catch (const std::exception& e) {
        core::Log::ErrorF("AssetRegistry: load exception: %s", e.what());
        return false;
    }
}

std::vector<AssetId> AssetRegistry::GetAllIds() const {
    std::vector<AssetId> ids;
    ids.reserve(m_assets.size());
    for (const auto& [id, md] : m_assets) {
        (void)md;
        ids.push_back(id);
    }
    return ids;
}

void AssetRegistry::Clear() {
    m_assets.clear();
    m_pathToId.clear();
    m_refCounts.clear();
    m_memoryUsage = 0;
    m_cache.Clear();
    core::Log::Info("AssetRegistry: cleared");
}

} // namespace assets
} // namespace eyelib

