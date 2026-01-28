#pragma once

#include "AnimationClip.h"
#include "BlendTree.h"
#include "Skeleton.h"
#include "StateMachine.h"

#include "eyelib/math/Types.h"

#include <memory>
#include <vector>

namespace eyelib {
namespace animation { class Animator; }
namespace api {

class Animator {
public:
    Animator();
    ~Animator();

    Animator(const Animator&) = delete;
    Animator& operator=(const Animator&) = delete;

    void SetSkeleton(std::shared_ptr<Skeleton> skeleton);
    void SetClip(std::shared_ptr<AnimationClip> clip);
    void SetStateMachine(std::shared_ptr<StateMachine> sm);
    void SetBlendTree(std::shared_ptr<BlendTree> tree);

    void Update(float deltaTime);
    const std::vector<eyelib::math::Mat4>& GetBoneMatrices() const;

    void SetParameter(const std::string& name, float value);
    void SetParameter(const std::string& name, bool value);
    void SetTrigger(const std::string& name);

    /**
     * @brief Set root motion bone name.
     * 
     * Root motion bone is typically "root", "Armature", or "Hips".
     * When set, root motion will be extracted from this bone.
     * 
     * @param boneName Name of the root motion bone (empty string to disable)
     */
    void SetRootMotionBone(const std::string& boneName);

    /**
     * @brief Get root motion delta transform.
     * 
     * Returns the delta transform of the root motion bone since last Update().
     * This should be applied to the character's world transform.
     * 
     * @return Root motion delta transform (position, rotation, scale)
     */
    struct RootMotionDelta {
        eyelib::math::Vec3 translation{0.0f};
        eyelib::math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        bool hasMotion = false;
    };
    RootMotionDelta GetRootMotionDelta() const;

    /**
     * @brief Enable or disable root motion extraction.
     * 
     * @param enabled True to extract root motion, false to disable
     */
    void SetRootMotionEnabled(bool enabled);

    /**
     * @brief Motion authority mode.
     * Determines how root motion is handled.
     */
    enum class MotionAuthority {
        AnimationDriven,  // Root motion drives character movement
        ControllerDriven, // Input/physics drives movement, root motion ignored
        Hybrid            // Both sources blended (explicit blend required)
    };

    /**
     * @brief Set motion authority mode.
     * 
     * @param authority Motion authority mode
     */
    void SetMotionAuthority(MotionAuthority authority);

    /**
     * @brief Get motion authority mode.
     */
    MotionAuthority GetMotionAuthority() const;

    static std::shared_ptr<Animator> WrapInternal(const std::shared_ptr<eyelib::animation::Animator>& animator);
    std::shared_ptr<eyelib::animation::Animator> GetInternal() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
