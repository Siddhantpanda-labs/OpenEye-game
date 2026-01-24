#pragma once

#include "ComponentHandles.h"

#include "eyelib/math/Types.h"

#include <vector>

namespace eyelib {
namespace ecs {
namespace components {

struct Animator {
    AnimatorHandle animator;
    SkeletonHandle skeleton;
    std::vector<math::Mat4> boneMatrices;
};

} // namespace components
} // namespace ecs
} // namespace eyelib
