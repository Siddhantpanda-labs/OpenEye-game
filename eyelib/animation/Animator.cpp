#include "Animator.h"

#include "eyelib/math/TransformUtils.h"
#include <algorithm>
#include <cmath>

namespace eyelib {
namespace animation {

void Animator::SetSkeleton(std::shared_ptr<Skeleton> skeleton) {
    m_skeleton = std::move(skeleton);
    const size_t boneCount = m_skeleton ? m_skeleton->BoneCount() : 0;
    m_pose.Resize(boneCount);
    m_boneMatrices.assign(boneCount, math::Mat4(1.0f));
    if (m_stateMachine) m_stateMachine->SetBoneCount(boneCount);
    
    // Reset root motion tracking if bone index is now invalid
    if (m_rootMotionBoneIndex >= 0 && static_cast<size_t>(m_rootMotionBoneIndex) >= boneCount) {
        m_rootMotionBoneIndex = -1;
        m_hasPreviousRootTransformBeforeCancel = false;
        m_rootMotionDelta.hasMotion = false;
    }
}

void Animator::SetStateMachine(std::shared_ptr<AnimationStateMachine> sm) {
    m_stateMachine = std::move(sm);
    if (m_stateMachine && m_skeleton) {
        m_stateMachine->SetBoneCount(m_skeleton->BoneCount());
    }
}

void Animator::SetBlendTree(std::shared_ptr<BlendTree> tree) {
    m_blendTree = std::move(tree);
}

void Animator::SetClip(std::shared_ptr<AnimationClip> clip) {
    m_clip = std::move(clip);
    m_time = 0.0f;
    // Reset root motion continuity only on clip change (first frame of clip)
    m_hasPreviousRootTransformBeforeCancel = false;
    m_rootMotionDelta.hasMotion = false;
    m_previousRootSampleTime = 0.0f;
}

void Animator::SetParameter(const std::string& name, float value) {
    m_floatParams[name] = value;
}

void Animator::SetParameter(const std::string& name, bool value) {
    m_boolParams[name] = value;
}

float Animator::GetParameter(const std::string& name) const {
    auto it = m_floatParams.find(name);
    return it != m_floatParams.end() ? it->second : 0.0f;
}

bool Animator::GetBoolParameter(const std::string& name) const {
    auto it = m_boolParams.find(name);
    return it != m_boolParams.end() ? it->second : false;
}

void Animator::SetTrigger(const std::string& name) {
    m_triggers[name] = true;
}

void Animator::Update(float dt) {
    m_time += dt;
    if (!m_skeleton) return;

    if (m_stateMachine) {
        for (const auto& [k, v] : m_boolParams) m_stateMachine->SetCondition(k, v);
        for (const auto& [k, v] : m_floatParams) m_stateMachine->SetCondition(k, v);
        for (const auto& [k, v] : m_triggers) {
            if (v) m_stateMachine->SetTrigger(k);
        }
        m_triggers.clear();
        if (m_stateMachine->GetCurrentState() < 0 && m_skeleton->BoneCount() > 0) {
            m_stateMachine->SetBoneCount(m_skeleton->BoneCount());
        }
        m_stateMachine->Update(dt);
        m_pose = m_stateMachine->GetCurrentPose();
    } else if (m_blendTree) {
        std::unordered_map<std::string, float> params;
        params.insert(m_floatParams.begin(), m_floatParams.end());
        m_pose = m_blendTree->Evaluate(params, m_time, m_skeleton->BoneCount());
    } else if (m_clip) {
        const size_t boneCount = m_skeleton->BoneCount();
        m_pose.Resize(boneCount);
        for (size_t i = 0; i < boneCount; ++i) {
            // IMPORTANT: When root motion is enabled, the root bone must NEVER receive
            // animated or bind-pose transforms. World motion owns the root.
            if (m_rootMotionEnabled && static_cast<int>(i) == m_rootMotionBoneIndex) {
                math::Transform identityPose;
                identityPose.position = math::Vec3(0.0f, 0.0f, 0.0f);
                identityPose.rotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
                identityPose.scale = math::Vec3(1.0f, 1.0f, 1.0f);
                m_pose.SetBoneTransform(i, identityPose);
                continue;
            }
            
            const Bone& b = m_skeleton->GetBone(i);
            math::Transform t;
            t.position = b.bindPosition;
            t.rotation = b.bindRotation;
            t.scale = b.bindScale;

            const auto* ch = m_clip->FindChannel(static_cast<int>(i));
            if (ch) {
                if (!ch->positionKeys.empty()) {
                    t.position = m_clip->SamplePosition(m_time, static_cast<int>(i));
                }
                if (!ch->rotationKeys.empty()) {
                    t.rotation = m_clip->SampleRotation(m_time, static_cast<int>(i));
                }
                if (!ch->scaleKeys.empty()) {
                    t.scale = m_clip->SampleScale(m_time, static_cast<int>(i));
                }
            }
            m_pose.SetBoneTransform(i, t);
        }
    }

    // Extract root motion if enabled (before building bone matrices)
    if (m_rootMotionEnabled && m_rootMotionBoneIndex >= 0) {
        ExtractRootMotion();
        
        // IMPORTANT: When root motion is enabled, the root bone must NEVER receive
        // animated or bind-pose transforms. World motion owns the root.
        // For clips, this is already done during pose evaluation, but we do it here for state machine/blend tree.
        if (static_cast<size_t>(m_rootMotionBoneIndex) < m_pose.GetBoneCount()) {
            math::Transform identityPose;
            identityPose.position = math::Vec3(0.0f, 0.0f, 0.0f);
            identityPose.rotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
            identityPose.scale = math::Vec3(1.0f, 1.0f, 1.0f);
            m_pose.SetBoneTransform(static_cast<size_t>(m_rootMotionBoneIndex), identityPose);
        }
    } else {
        m_rootMotionDelta.hasMotion = false;
    }

    BuildBoneMatrices();
}

void Animator::RebuildPose() {
    if (!m_skeleton) return;
    m_pose.Resize(m_skeleton->BoneCount());
}

void Animator::SetRootMotionBone(int boneIndex) {
    m_rootMotionBoneIndex = boneIndex;
    m_hasPreviousRootTransformBeforeCancel = false;
    m_rootMotionDelta.hasMotion = false;
    m_previousRootSampleTime = 0.0f;
}

void Animator::SetRootMotionEnabled(bool enabled) {
    m_rootMotionEnabled = enabled;
    if (!enabled) {
        m_hasPreviousRootTransformBeforeCancel = false;
        m_rootMotionDelta.hasMotion = false;
        m_previousRootSampleTime = 0.0f;
    }
}

void Animator::ExtractRootMotion() {
    if (!m_skeleton || m_rootMotionBoneIndex < 0) return;
    if (static_cast<size_t>(m_rootMotionBoneIndex) >= m_skeleton->BoneCount()) return;
    
    math::Transform animatedRootTransform;
    auto sampleRootAtTime = [&](float time) -> math::Mat4 {
        math::Transform t;
        if (m_clip) {
            const Bone& rootBone = m_skeleton->GetBone(static_cast<size_t>(m_rootMotionBoneIndex));
            t.position = rootBone.bindPosition;
            t.rotation = rootBone.bindRotation;
            t.scale = rootBone.bindScale;
            const auto* ch = m_clip->FindChannel(m_rootMotionBoneIndex);
            if (ch) {
                if (!ch->positionKeys.empty()) {
                    t.position = m_clip->SamplePosition(time, m_rootMotionBoneIndex);
                }
                if (!ch->rotationKeys.empty()) {
                    t.rotation = m_clip->SampleRotation(time, m_rootMotionBoneIndex);
                }
                if (!ch->scaleKeys.empty()) {
                    t.scale = m_clip->SampleScale(time, m_rootMotionBoneIndex);
                }
            }
        } else {
            t = m_pose.GetBoneTransform(static_cast<size_t>(m_rootMotionBoneIndex));
        }
        return math::Compose(t.position, t.rotation, t.scale);
    };
    
    // CRITICAL: For clips, sample animation directly (pose already has identity)
    // For state machine/blend tree, read from pose (they haven't been overridden yet)
    if (m_clip) {
        // Sample animation directly - pose already has identity for root bone
        const float duration = m_clip->Duration();
        const bool loop = m_clip->Loop();
        const auto normalizeTime = [&](float time) -> float {
            if (duration <= 0.0f) return 0.0f;
            if (!loop) {
                return std::min(std::max(time, 0.0f), duration);
            }
            float t = std::fmod(time, duration);
            if (t < 0.0f) t += duration;
            return t;
        };

        const float prevTime = m_previousRootSampleTime;
        const float currTime = m_time;
        const float prevT = normalizeTime(prevTime);
        const float currT = normalizeTime(currTime);

        const math::Mat4 prevMat = sampleRootAtTime(prevT);
        const math::Mat4 currMat = sampleRootAtTime(currT);

        math::Mat4 deltaMat{1.0f};
        if (m_hasPreviousRootTransformBeforeCancel && loop && currT < prevT) {
            // Handle wrap: prev->end + start->curr
            const math::Mat4 endMat = sampleRootAtTime(duration);
            const math::Mat4 startMat = sampleRootAtTime(0.0f);
            const math::Mat4 prevToEnd = endMat * glm::inverse(prevMat);
            const math::Mat4 startToCurr = currMat * glm::inverse(startMat);
            deltaMat = startToCurr * prevToEnd;
        } else if (m_hasPreviousRootTransformBeforeCancel) {
            deltaMat = currMat * glm::inverse(prevMat);
        }

        // Extract translation and rotation from delta
        math::Vec3 deltaPos, deltaScale;
        math::Quat deltaRot;
        math::Decompose(deltaMat, deltaPos, deltaRot, deltaScale);

        if (m_hasPreviousRootTransformBeforeCancel) {
            m_rootMotionDelta.translation = deltaPos;
            m_rootMotionDelta.rotation = deltaRot;
            m_rootMotionDelta.hasMotion = true;
        } else {
            m_rootMotionDelta.translation = math::Vec3(0.0f);
            m_rootMotionDelta.rotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
            m_rootMotionDelta.hasMotion = false;
        }

        m_previousRootSampleTime = currTime;
        m_previousRootTransformBeforeCancel = currMat;
        m_hasPreviousRootTransformBeforeCancel = true;
        return;
    } else {
        // For state machine/blend tree: read from pose (before we override it)
        // Note: These paths override root to identity AFTER pose evaluation, so we read it here
        if (m_pose.GetBoneCount() <= static_cast<size_t>(m_rootMotionBoneIndex)) {
            m_rootMotionDelta.hasMotion = false;
            return;
        }
        animatedRootTransform = m_pose.GetBoneTransform(static_cast<size_t>(m_rootMotionBoneIndex));
    }
    
    // Get current root transform in local space (WITH root motion)
    math::Mat4 currentRootMat = math::Compose(
        animatedRootTransform.position, animatedRootTransform.rotation, animatedRootTransform.scale);
    
    // Calculate delta from previous frame's transform (before cancellation)
    if (m_hasPreviousRootTransformBeforeCancel) {
        // Calculate delta: current * inverse(previous)
        math::Mat4 deltaMat = currentRootMat * glm::inverse(m_previousRootTransformBeforeCancel);
        
        // Extract translation and rotation from delta
        math::Vec3 deltaPos, deltaScale;
        math::Quat deltaRot;
        math::Decompose(deltaMat, deltaPos, deltaRot, deltaScale);
        
        m_rootMotionDelta.translation = deltaPos;
        m_rootMotionDelta.rotation = deltaRot;
        m_rootMotionDelta.hasMotion = true;
    } else {
        // First frame - no delta
        m_rootMotionDelta.translation = math::Vec3(0.0f);
        m_rootMotionDelta.rotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
        m_rootMotionDelta.hasMotion = false;
    }
    
    // Store current root transform (WITH root motion) for next frame delta calculation
    m_previousRootTransformBeforeCancel = currentRootMat;
    m_hasPreviousRootTransformBeforeCancel = true;
}

void Animator::BuildBoneMatrices() {
    if (!m_skeleton) return;
    const size_t count = m_skeleton->BoneCount();
    if (m_pose.GetBoneCount() < count) m_pose.Resize(count);
    if (m_boneMatrices.size() != count) m_boneMatrices.assign(count, math::Mat4(1.0f));

    std::vector<math::Mat4> local(count, math::Mat4(1.0f));
    for (size_t i = 0; i < count; ++i) {
        const auto& t = m_pose.GetBoneTransform(i);
        local[i] = math::Compose(t.position, t.rotation, t.scale);
    }

    std::vector<math::Mat4> global(count, math::Mat4(1.0f));
    std::vector<uint8_t> computed(count, 0);

    // Root motion requires the skeleton root to be a virtual identity node.
    // World motion owns root translation and rotation.
    std::function<void(size_t)> computeGlobal = [&](size_t idx) {
        if (computed[idx]) return;
        const Bone& b = m_skeleton->GetBone(idx);
        
        // CRITICAL: Root motion bone must have identity global transform
        if (m_rootMotionEnabled && static_cast<int>(idx) == m_rootMotionBoneIndex) {
            global[idx] = math::Mat4(1.0f); // Force identity
            computed[idx] = 1;
            return;
        }
        
        if (b.parentIndex >= 0) {
            const size_t parent = static_cast<size_t>(b.parentIndex);
            if (parent < count) {
                computeGlobal(parent);
                global[idx] = global[parent] * local[idx];
            } else {
                global[idx] = local[idx];
            }
        } else {
            global[idx] = local[idx];
        }
        computed[idx] = 1;
    };

    for (size_t i = 0; i < count; ++i) {
        computeGlobal(i);
        const Bone& b = m_skeleton->GetBone(i);
        
        // CRITICAL: Root motion bone final matrix must be identity-based
        // RULE: Root motion lives ONLY in world/entity space.
        // RULE: Root bone is always visual identity when root motion is enabled.
        // RULE: Skeleton root is virtual and has no spatial meaning.
        if (m_rootMotionEnabled && static_cast<int>(i) == m_rootMotionBoneIndex) {
            // Root motion bone: completely neutral - no spatial transformation at all
            // Root motion is extracted and applied to world transform, skeleton must not contribute
            // Use identity matrix to ensure root bone doesn't add any offset to skinning
            // NOTE: This assumes root bone vertices are weighted to child bones, not root itself
            m_boneMatrices[i] = math::Mat4(1.0f);
        } else {
            m_boneMatrices[i] = m_skeleton->GetGlobalInverseTransform() * global[i] * b.inverseBindMatrix;
        }
    }
}

} // namespace animation
} // namespace eyelib
