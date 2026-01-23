#pragma once

#include "eyelib/animation/AnimationClip.h"
#include "eyelib/animation/Skeleton.h"

#include <memory>

namespace eyelib {
namespace animation {

/**
 * @brief Minimal animator state (plays one clip on one skeleton).
 *
 * Full state machines/blending will be added later.
 */
class Animator {
public:
    void SetSkeleton(std::shared_ptr<Skeleton> skeleton) { m_skeleton = std::move(skeleton); }
    void SetClip(std::shared_ptr<AnimationClip> clip) { m_clip = std::move(clip); m_time = 0.0f; }

    void Update(float dt) { m_time += dt; }

private:
    std::shared_ptr<Skeleton> m_skeleton{};
    std::shared_ptr<AnimationClip> m_clip{};
    float m_time = 0.0f;
};

} // namespace animation
} // namespace eyelib

