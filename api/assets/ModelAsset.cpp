#include "api/assets/ModelAsset.h"

namespace eyelib {
namespace api {

const std::vector<std::shared_ptr<Mesh>>& ModelAsset::GetMeshes() const {
    return m_meshes;
}

const std::vector<std::shared_ptr<Material>>& ModelAsset::GetMaterials() const {
    return m_materials;
}

std::shared_ptr<Skeleton> ModelAsset::GetSkeleton() const {
    return m_skeleton;
}

std::shared_ptr<AnimationClip> ModelAsset::GetClip() const {
    return m_clip;
}

std::shared_ptr<ModelAsset> ModelAsset::WrapInternal(
    std::vector<std::shared_ptr<Mesh>> meshes,
    std::vector<std::shared_ptr<Material>> materials,
    std::shared_ptr<Skeleton> skeleton,
    std::shared_ptr<AnimationClip> clip) {
    auto asset = std::make_shared<ModelAsset>();
    asset->m_meshes = std::move(meshes);
    asset->m_materials = std::move(materials);
    asset->m_skeleton = std::move(skeleton);
    asset->m_clip = std::move(clip);
    return asset;
}

} // namespace api
} // namespace eyelib
