#pragma once

#include "eyelib/animation/AnimationClip.h"
#include "eyelib/animation/Skeleton.h"

#include <memory>
#include <vector>

namespace eyelib {
namespace assets {

struct AnimationAsset {
    std::shared_ptr<animation::Skeleton> skeleton;
    std::vector<std::shared_ptr<animation::AnimationClip>> clips;
};

} // namespace assets
} // namespace eyelib
