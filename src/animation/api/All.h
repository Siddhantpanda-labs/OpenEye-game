#pragma once

/**
 * @brief Convenience header that includes all Animation API headers
 * 
 * Apps can include this single header to get all Animation functionality:
 * #include "animation/api/All.h"
 */

#include "../AnimationSystem.h"
#include "../Skeleton.h"
#include "../AnimationClip.h"

namespace OpenEye {
namespace Animation {
namespace API {

using AnimationSystem = OpenEye::Animation::AnimationSystem;
using Skeleton = OpenEye::Animation::Skeleton;
using AnimationClip = OpenEye::Animation::AnimationClip;

} // namespace API
} // namespace Animation
} // namespace OpenEye

