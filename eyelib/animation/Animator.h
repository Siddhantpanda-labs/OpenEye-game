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
 * @brief Root motion delta structure.
 */
struct RootMotionDelta {
    math::Vec3 translation{0.0f};
    math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    bool hasMotion = false;
};

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
    /** Global bone transforms in model space (for debug drawing). */
    const std::vector<math::Mat4>& GetBoneGlobalMatrices() const { return m_globalBoneMatrices; }

    void SetParameter(const std::string& name, float value);
    void SetParameter(const std::string& name, bool value);
    float GetParameter(const std::string& name) const;
    bool GetBoolParameter(const std::string& name) const;
    void SetTrigger(const std::string& name);

    // Root motion support
    void SetRootMotionBone(int boneIndex);
    void SetRootMotionEnabled(bool enabled);
    bool IsRootMotionEnabled() const { return m_rootMotionEnabled; }
    int GetRootMotionBoneIndex() const { return m_rootMotionBoneIndex; }
    RootMotionDelta GetRootMotionDelta() const { return m_rootMotionDelta; }

private:
    void RebuildPose();
    void BuildBoneMatrices();
    void ExtractRootMotion();

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
    std::vector<math::Mat4> m_globalBoneMatrices;

    // Root motion tracking
    int m_rootMotionBoneIndex = -1;
    bool m_rootMotionEnabled = false;
    RootMotionDelta m_rootMotionDelta;
    math::Mat4 m_previousRootTransformBeforeCancel{1.0f};
    bool m_hasPreviousRootTransformBeforeCancel = false;
    float m_previousRootSampleTime = 0.0f;
};

} // namespace animation
} // namespace eyelib

