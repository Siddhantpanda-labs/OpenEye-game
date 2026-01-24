#include "MeshLoader.h"

#include "eyelib/assets/loaders/MeshLoader.h"

namespace eyelib {
namespace api {

namespace {
AssetId ToApi(const assets::AssetId& id) {
    AssetId out;
    out.high = id.High();
    out.low = id.Low();
    return out;
}
} // namespace

AssetId MeshLoader::LoadMesh(const std::string& path) {
    return ToApi(assets::MeshLoader::LoadMesh(path));
}

void MeshLoader::LoadMeshAsync(const std::string& path, LoadCallback callback) {
    assets::MeshLoader::LoadMeshAsync(path, [callback](assets::AssetId id) {
        if (callback) callback(ToApi(id));
    });
}

} // namespace api
} // namespace eyelib
