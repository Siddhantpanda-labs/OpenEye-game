#pragma once

#include "eyelib/animation/AnimationClip.h"
#include "eyelib/animation/Skeleton.h"
#include "eyelib/math/Types.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Mesh.h"

#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace assets {

struct LoadedModel {
    std::vector<std::shared_ptr<render::Mesh>> meshes;
    std::vector<std::shared_ptr<render::Material>> materials;
    std::vector<math::Mat4> meshTransforms;
    std::shared_ptr<animation::Skeleton> skeleton;
    std::shared_ptr<animation::AnimationClip> clip;
};

class ModelLoader {
public:
    static LoadedModel LoadModel(const std::string& path);
};

} // namespace assets
} // namespace eyelib
