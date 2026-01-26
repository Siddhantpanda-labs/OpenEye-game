#include "api/assets/loaders/ModelLoader.h"

#include "api/animation/AnimationClip.h"
#include "api/animation/Skeleton.h"
#include "api/assets/ModelAsset.h"
#include "api/render/Material.h"
#include "api/render/Mesh.h"
#include "eyelib/assets/loaders/ModelLoader.h"

namespace eyelib {
namespace api {

std::shared_ptr<ModelAsset> ModelLoader::LoadModel(const std::string& path) {
    // Delegate to internal implementation
    auto loaded = assets::ModelLoader::LoadModel(path);
    
    // Wrap internal types in API types
    std::vector<std::shared_ptr<Mesh>> apiMeshes;
    apiMeshes.reserve(loaded.meshes.size());
    for (const auto& m : loaded.meshes) {
        apiMeshes.push_back(Mesh::WrapInternal(m));
    }

    std::vector<std::shared_ptr<Material>> apiMaterials;
    apiMaterials.reserve(loaded.materials.size());
    for (const auto& m : loaded.materials) {
        apiMaterials.push_back(Material::WrapInternal(m));
    }

    return ModelAsset::WrapInternal(
        std::move(apiMeshes),
        std::move(apiMaterials),
        std::move(loaded.meshTransforms),
        Skeleton::WrapInternal(loaded.skeleton),
        AnimationClip::WrapInternal(loaded.clip)
    );
}

} // namespace api
} // namespace eyelib
