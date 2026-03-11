#pragma once

#include "api/animation/AnimationClip.h"
#include "api/animation/Skeleton.h"
#include "api/render/Material.h"
#include "api/render/Mesh.h"
#include "eyelib/math/Types.h"

#include <memory>
#include <vector>

namespace eyelib {
namespace api {

class ModelAsset {
public:
    ModelAsset() = default;

    const std::vector<std::shared_ptr<Mesh>>& GetMeshes() const;
    const std::vector<std::shared_ptr<Material>>& GetMaterials() const;
    const std::vector<math::Mat4>& GetMeshTransforms() const;
    std::shared_ptr<Skeleton> GetSkeleton() const;
    std::shared_ptr<AnimationClip> GetClip() const;

    static std::shared_ptr<ModelAsset> WrapInternal(
        std::vector<std::shared_ptr<Mesh>> meshes,
        std::vector<std::shared_ptr<Material>> materials,
        std::vector<math::Mat4> meshTransforms,
        std::shared_ptr<Skeleton> skeleton,
        std::shared_ptr<AnimationClip> clip);

private:
    std::vector<std::shared_ptr<Mesh>> m_meshes;
    std::vector<std::shared_ptr<Material>> m_materials;
    std::vector<math::Mat4> m_meshTransforms;
    std::shared_ptr<Skeleton> m_skeleton;
    std::shared_ptr<AnimationClip> m_clip;
};

} // namespace api
} // namespace eyelib
