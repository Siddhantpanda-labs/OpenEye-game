#pragma once

#include "asset/core/asset_type.h"
#include "asset/core/guid.h"
#include "asset/core/import_mesh.h"
#include "asset/core/import_animation.h"

#include "asset/readers/material_reader.h"
#include "asset/readers/texture_reader.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace asset {

class AssetDatabase;

/**
 * Runtime asset loader: GUID -> path (via database) -> reader -> cached result.
 * Bridge between engine and assets. Set database and asset root before loading.
 */
class AssetManager {
public:
    static AssetManager& Get();

    void SetDatabase(AssetDatabase* db) { m_database = db; }
    void SetAssetRoot(const std::string& root) { m_assetRoot = root; }
    AssetDatabase* GetDatabase() const { return m_database; }

    /** Resolve GUID to full path (assetRoot + relative path). Returns empty if not found. */
    std::string ResolvePath(const Guid& guid) const;

    std::shared_ptr<ImportMesh> LoadMesh(const Guid& guid);
    std::shared_ptr<LoadedMaterial> LoadMaterial(const Guid& guid);
    std::shared_ptr<LoadedTexture> LoadTexture(const Guid& guid);
    std::shared_ptr<ImportAnimation> LoadAnimation(const Guid& guid);

    /** Unload from cache. Recursively unloads dependencies that have no other loaded dependents. */
    void UnloadAsset(const Guid& guid);

    /** True if this GUID is currently in any cache. */
    bool IsLoaded(const Guid& guid) const;

    void ClearCache();

private:
    AssetManager() = default;

    std::string FullPath(const std::string& relativePath) const;
    void LoadDependencies(const Guid& guid, AssetType type);

    AssetDatabase* m_database = nullptr;
    std::string m_assetRoot;

    std::unordered_map<Guid, std::shared_ptr<ImportMesh>, Guid::Hash> m_meshes;
    std::unordered_map<Guid, std::shared_ptr<LoadedMaterial>, Guid::Hash> m_materials;
    std::unordered_map<Guid, std::shared_ptr<LoadedTexture>, Guid::Hash> m_textures;
    std::unordered_map<Guid, std::shared_ptr<ImportAnimation>, Guid::Hash> m_animations;
};

} // namespace asset
