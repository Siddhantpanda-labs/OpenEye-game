#pragma once

#include "AnimationClip.h"
#include "BlendTree.h"
#include "Pose.h"
#include "Skeleton.h"
#include "AnimationStateMachine.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace animation {

/**
 * @brief Animator controller: state machine or blend tree driven.
 */
class Animator {
public:
    void SetSkeleton(std::shared_ptr<Skeleton> skeleton);
    void SetStateMachine(std::shared_ptr<AnimationStateMachine> sm);
    void SetBlendTree(std::shared_ptr<BlendTree> tree);
    void SetClip(std::shared_ptr<AnimationClip> clip);

    void Update(float dt);
    const Pose& GetPose() const { return m_pose; }
    const std::vector<math::Mat4>& GetBoneMatrices() const { return m_boneMatrices; }

    void SetParameter(const std::string& name, float value);
    void SetParameter(const std::string& name, bool value);
    float GetParameter(const std::string& name) const;
    bool GetBoolParameter(const std::string& name) const;
    void SetTrigger(const std::string& name);

private:
    void RebuildPose();
    void BuildBoneMatrices();

    std::shared_ptr<Skeleton> m_skeleton{};
    std::shared_ptr<AnimationClip> m_clip{};
    std::shared_ptr<AnimationStateMachine> m_stateMachine{};
    std::shared_ptr<BlendTree> m_blendTree{};

    std::unordered_map<std::string, float> m_floatParams;
    std::unordered_map<std::string, bool> m_boolParams;
    std::unordered_map<std::string, bool> m_triggers;

    float m_time = 0.0f;
    Pose m_pose;
    std::vector<math::Mat4> m_boneMatrices;
};

} // namespace animation
} // namespace eyelib

