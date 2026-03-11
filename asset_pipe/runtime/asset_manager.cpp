#include "asset_pipe/runtime/asset_manager.h"
#include "asset_pipe/database/asset_database.h"
#include "asset_pipe/readers/mesh_reader.h"
#include "asset_pipe/readers/animation_reader.h"
#include "asset_pipe/readers/oea_reader.h"
#include "asset_pipe/core/oea_format.h"

#include <algorithm>
#include <iostream>

namespace asset_pipe {

AssetManager& AssetManager::Get() {
    static AssetManager s_instance;
    return s_instance;
}

std::string AssetManager::FullPath(const std::string& relativePath) const {
    if (m_assetRoot.empty()) return relativePath;
    std::string p = relativePath;
    while (!p.empty() && (p.front() == '/' || p.front() == '\\')) p.erase(0, 1);
    return m_assetRoot + "/" + p;
}

std::string AssetManager::ResolvePath(const Guid& guid) const {
    if (!m_database) return {};
    if (!m_database->AssetExists(guid)) {
        std::cerr << "[AssetManager] ResolvePath: GUID not in database: " << guid.ToString() << std::endl;
        return {};
    }
    std::string rel = m_database->GetPath(guid);
    if (rel.empty()) return {};
    return FullPath(rel);
}

void AssetManager::LoadDependencies(const Guid& guid, AssetType type) {
    if (!m_database) return;
    const AssetInfo* info = m_database->GetAssetInfo(guid);
    if (!info) return;
    for (const Guid& dep : info->dependencies) {
        const AssetInfo* depInfo = m_database->GetAssetInfo(dep);
        if (!depInfo) continue;
        if (type == AssetType::Mesh && depInfo->type == AssetType::Material)
            LoadMaterial(dep);
        else if (type == AssetType::Material && depInfo->type == AssetType::Texture)
            LoadTexture(dep);
    }
}

bool AssetManager::IsLoaded(const Guid& guid) const {
    return m_meshes.count(guid) || m_materials.count(guid) ||
           m_textures.count(guid) || m_animations.count(guid);
}

std::shared_ptr<ImportMesh> AssetManager::LoadMesh(const Guid& guid) {
    if (!guid.IsValid()) return nullptr;
    auto it = m_meshes.find(guid);
    if (it != m_meshes.end()) return it->second;

    std::string path = ResolvePath(guid);
    if (path.empty()) return nullptr;

    auto mesh = std::make_shared<ImportMesh>();
    bool ok = false;
    if (path.size() >= 6) {
        std::string ext = path.substr(path.size() - 6);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".omesh") {
            std::vector<OeaEntry> entries;
            if (OeaReader::ReadIndex(path, entries)) {
                for (const auto& e : entries) {
                    if (e.type == OeaFormat::Entry_Mesh) {
                        std::vector<uint8_t> data;
                        if (OeaReader::ReadEntryData(path, e.name, data) && MeshReader::Read(data, *mesh)) {
                            ok = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    if (!ok) ok = MeshReader::Read(path, *mesh);
    if (!ok) return nullptr;
    m_meshes[guid] = mesh;
    LoadDependencies(guid, AssetType::Mesh);
    return mesh;
}

std::shared_ptr<LoadedMaterial> AssetManager::LoadMaterial(const Guid& guid) {
    if (!guid.IsValid()) return nullptr;
    auto it = m_materials.find(guid);
    if (it != m_materials.end()) return it->second;

    std::string path = ResolvePath(guid);
    if (path.empty()) return nullptr;

    auto mat = std::make_shared<LoadedMaterial>();
    if (!MaterialReader::Read(path, *mat)) return nullptr;
    m_materials[guid] = mat;
    LoadDependencies(guid, AssetType::Material);
    return mat;
}

std::shared_ptr<LoadedTexture> AssetManager::LoadTexture(const Guid& guid) {
    if (!guid.IsValid()) return nullptr;
    auto it = m_textures.find(guid);
    if (it != m_textures.end()) return it->second;

    std::string path = ResolvePath(guid);
    if (path.empty()) return nullptr;

    auto tex = std::make_shared<LoadedTexture>();
    if (!TextureReader::Read(path, *tex)) return nullptr;
    m_textures[guid] = tex;
    return tex;
}

std::shared_ptr<ImportAnimation> AssetManager::LoadAnimation(const Guid& guid) {
    if (!guid.IsValid()) return nullptr;
    auto it = m_animations.find(guid);
    if (it != m_animations.end()) return it->second;

    std::string path = ResolvePath(guid);
    if (path.empty()) return nullptr;

    auto anim = std::make_shared<ImportAnimation>();
    if (!AnimationReader::Read(path, *anim)) return nullptr;
    m_animations[guid] = anim;
    return anim;
}

void AssetManager::UnloadAsset(const Guid& guid) {
    if (!guid.IsValid()) return;
    bool wasMesh = m_meshes.erase(guid) != 0;
    bool wasMat = m_materials.erase(guid) != 0;
    bool wasTex = m_textures.erase(guid) != 0;
    bool wasAnim = m_animations.erase(guid) != 0;
    if (!wasMesh && !wasMat && !wasTex && !wasAnim) return;

    if (!m_database) return;
    for (const Guid& dep : m_database->GetDependencies(guid)) {
        std::vector<Guid> dependents = m_database->GetDependents(dep);
        bool anyOtherLoaded = false;
        for (const Guid& depOf : dependents) {
            if (depOf == guid) continue;
            if (IsLoaded(depOf)) { anyOtherLoaded = true; break; }
        }
        if (!anyOtherLoaded) UnloadAsset(dep);
    }
}

void AssetManager::ClearCache() {
    m_meshes.clear();
    m_materials.clear();
    m_textures.clear();
    m_animations.clear();
}

} // namespace asset_pipe
