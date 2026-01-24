#include "MeshLoader.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/render/resources/MeshLoader.h"

#include <thread>

namespace eyelib {
namespace assets {

AssetId MeshLoader::LoadMesh(const std::string& path) {
    auto& manager = AssetManager::Get();
    const AssetId id = manager.Registry().RegisterAsset(path, "Mesh");
    auto mesh = render::MeshLoader::LoadSingle(path);
    if (mesh) {
        mesh->SetAssetId(id);
        manager.Cache().Store(id, mesh, "Mesh");
    }
    return id;
}

void MeshLoader::LoadMeshAsync(const std::string& path, LoadCallback callback) {
    std::thread([path, callback]() {
        const AssetId id = LoadMesh(path);
        if (callback) callback(id);
    }).detach();
}

} // namespace assets
} // namespace eyelib
