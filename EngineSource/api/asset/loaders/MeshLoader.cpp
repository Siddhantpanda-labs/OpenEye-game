#include "MeshLoader.h"
#include "api/asset/AssetRegistry.h"

namespace eyelib {
namespace api {

AssetId MeshLoader::LoadMesh(AssetRegistry* registry, const std::string& path) {
    if (!registry) return AssetId::Invalid();
    registry->LoadAsset(path, AssetType::Mesh);
    return registry->GetId(path);
}

AssetId MeshLoader::LoadMesh(const std::string& path) {
    (void)path;
    return AssetId::Invalid();
}

void MeshLoader::LoadMeshAsync(const std::string& path, LoadCallback callback) {
    if (callback) callback(AssetId::Invalid());
}

} // namespace api
} // namespace eyelib
