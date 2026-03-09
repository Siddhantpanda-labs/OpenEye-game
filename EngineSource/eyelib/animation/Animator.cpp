#include "Animator.h"
#include "BlendSystem.h"
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
    if (m_stateMachine) {
        m_stateMachine->SetBoneCount(boneCount);
        m_stateMachine->SetSkeleton(m_skeleton.get());
    }
    
    // Reset root motion tracking if bone index is now invalid
    if (m_rootMotionBoneIndex >= 0 && static_cast<size_t>(m_rootMotionBoneIndex) >= boneCount) {
        m_rootMotionBoneIndex = -1;
        m_hasPreviousRootTransformBeforeCancel = false;
        m_rootMotionDelta.hasMotion = false;
    }
}

void Animator::SetStateMachine(std::shared_ptr<AnimationStateMachine> sm) {
    m_stateMachine = std::move(sm);
    m_previousRootMotionState = -1;
    m_previousRootMotionStateTime = 0.0f;
    if (m_stateMachine && m_skeleton) {
        m_stateMachine->SetBoneCount(m_skeleton->BoneCount());
        m_stateMachine->SetSkeleton(m_skeleton.get());
    }
}

void Animator::SetBlendTree(std::shared_ptr<BlendTree> tree) {
    m_blendTree = std::move(tree);
}

void Animator::SetClip(std::shared_ptr<AnimationClip> clip) {
    // Only reset if clip actually changed
    if (m_clip != clip) {
        m_clip = std::move(clip);
        m_time = 0.0f;
        // Reset root motion continuity only on clip change (first frame of clip)
        // This prevents snapping when switching animations
        m_hasPreviousRootTransformBeforeCancel = false;
        m_rootMotionDelta.hasMotion = false;
        m_previousRootSampleTime = 0.0f;
    }
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
            const Bone& b = m_skeleton->GetBone(i);
            
            // IMPORTANT: When root motion is enabled, handle root motion bone specially
            // The root motion bone (usually Hips) needs its translation extracted but kept at bind pose for skeleton integrity
            // The actual root bone (parentIndex == -1, usually RootNode) should be completely neutralized
            bool isActualRootBone = false;
            bool isRootMotionBone = false;
            
            if (m_rootMotionEnabled) {
                // CRITICAL: Always neutralize the actual root bone (parentIndex == -1) completely
                // This is usually a dummy root node (e.g., "RootNode") that has no spatial meaning
                if (b.parentIndex == -1) {
                    isActualRootBone = true;
                }
                // Root motion bone: Keep bind pose translation, but allow rotation/scale from animation
                // This maintains skeleton hierarchy while extracting translation for root motion
                else if (static_cast<int>(i) == m_rootMotionBoneIndex && m_rootMotionBoneIndex >= 0) {
                    isRootMotionBone = true;
                }
            }
            
            if (isActualRootBone) {
                // Actual root bone: Completely neutralize (identity transform)
                math::Transform identityPose;
                identityPose.position = math::Vec3(0.0f, 0.0f, 0.0f);
                identityPose.rotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
                identityPose.scale = math::Vec3(1.0f, 1.0f, 1.0f);
                m_pose.SetBoneTransform(i, identityPose);
                continue;
            } else if (isRootMotionBone) {
                // Root motion bone: Keep bind pose translation, but use animation for rotation/scale
                // This maintains skeleton integrity while allowing root motion extraction
                math::Transform rootMotionPose;
                rootMotionPose.position = b.bindPosition; // Keep bind pose translation
                rootMotionPose.rotation = b.bindRotation;  // Start with bind rotation
                rootMotionPose.scale = b.bindScale;        // Start with bind scale
                
                // Apply animation rotation and scale (but NOT translation - that's extracted as root motion)
                const auto* ch = m_clip->FindChannel(static_cast<int>(i));
                if (ch) {
                    if (!ch->rotationKeys.empty()) {
                        rootMotionPose.rotation = m_clip->SampleRotation(m_time, static_cast<int>(i));
                    }
                    if (!ch->scaleKeys.empty()) {
                        rootMotionPose.scale = m_clip->SampleScale(m_time, static_cast<int>(i));
                    }
                    // Translation is NOT applied here - it's extracted as root motion delta
                }
                
                m_pose.SetBoneTransform(i, rootMotionPose);
                continue;
            }
            
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
    if (m_rootMotionEnabled) {
        if (m_rootMotionBoneIndex >= 0) {
            ExtractRootMotion();
            // In-place: keep root motion bone at bind position for skinning, but keep animated rotation/scale.
            // Always pin to bind (including during transition) so the model never jumps in position.
            const size_t rmb = static_cast<size_t>(m_rootMotionBoneIndex);
            if (rmb < m_pose.GetBoneCount()) {
                const Bone& b = m_skeleton->GetBone(rmb);
                math::Transform inPlace = m_pose.GetBoneTransform(rmb);
                inPlace.position = b.bindPosition;
                m_pose.SetBoneTransform(rmb, inPlace);
            }
        } else {
            // Root motion enabled but bone index not set - neutralize actual root bone anyway
            // This is a safety measure to prevent movement when root motion is enabled
            if (m_skeleton) {
                const size_t boneCount = m_skeleton->BoneCount();
                for (size_t i = 0; i < boneCount; ++i) {
                    const Bone& b = m_skeleton->GetBone(i);
                    if (b.parentIndex == -1) {
                        // Found root bone - neutralize it
                        math::Transform identityPose;
                        identityPose.position = math::Vec3(0.0f, 0.0f, 0.0f);
                        identityPose.rotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
                        identityPose.scale = math::Vec3(1.0f, 1.0f, 1.0f);
                        m_pose.SetBoneTransform(i, identityPose);
                        break;
                    }
                }
            }
            m_rootMotionDelta.hasMotion = false;
        }
        
        // CRITICAL: Always neutralize actual root bone (parentIndex == -1) regardless of rootMotionBoneIndex
        // This ensures the actual root bone (dummy root node) never causes movement when root motion is enabled
        // NOTE: The root motion bone (usually Hips) is NOT neutralized here - it keeps its bind pose translation
        if (m_skeleton) {
            const size_t boneCount = m_skeleton->BoneCount();
            bool rootFound = false;
            for (size_t i = 0; i < boneCount; ++i) {
                const Bone& b = m_skeleton->GetBone(i);
                // Only neutralize the actual root bone (parentIndex == -1), NOT the root motion bone
                if (b.parentIndex == -1 && static_cast<int>(i) != m_rootMotionBoneIndex) {
                    math::Transform identityPose;
                    identityPose.position = math::Vec3(0.0f, 0.0f, 0.0f);
                    identityPose.rotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
                    identityPose.scale = math::Vec3(1.0f, 1.0f, 1.0f);
                    m_pose.SetBoneTransform(i, identityPose);
                    rootFound = true;
                    break;
                }
            }
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
    m_previousRootMotionState = -1;
    m_previousRootMotionStateTime = 0.0f;
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
    
    const Bone& rootBone = m_skeleton->GetBone(static_cast<size_t>(m_rootMotionBoneIndex));
    math::Transform animatedRootTransform;
    auto sampleRootAtTime = [&](float time) -> math::Mat4 {
        math::Transform t;
         if (m_clip) {
            // Sample animation values directly (Assimp stores absolute local transforms)
            const auto* ch = m_clip->FindChannel(m_rootMotionBoneIndex);
            if (ch) {
                if (!ch->positionKeys.empty()) {
                    t.position = m_clip->SamplePosition(time, m_rootMotionBoneIndex);
                } else {
                    t.position = rootBone.bindPosition;
                }
                if (!ch->rotationKeys.empty()) {
                    t.rotation = m_clip->SampleRotation(time, m_rootMotionBoneIndex);
                } else {
                    t.rotation = rootBone.bindRotation;
                }
                if (!ch->scaleKeys.empty()) {
                    t.scale = m_clip->SampleScale(time, m_rootMotionBoneIndex);
                } else {
                    t.scale = rootBone.bindScale;
                }
            } else {
                t.position = rootBone.bindPosition;
                t.rotation = rootBone.bindRotation;
                t.scale = rootBone.bindScale;
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

        math::Vec3 deltaPos(0.0f);
        if (m_hasPreviousRootTransformBeforeCancel && loop && currT < prevT) {
            // Handle wrap: position delta = (curr - start) + (end - prev)
            const float endTime = (duration > 0.0f) ? std::nextafter(duration, 0.0f) : duration;
            const math::Mat4 endMat = sampleRootAtTime(endTime);
            const math::Mat4 startMat = sampleRootAtTime(0.0f);
            deltaPos = (math::Vec3(currMat[3][0], currMat[3][1], currMat[3][2]) - math::Vec3(startMat[3][0], startMat[3][1], startMat[3][2]))
                    + (math::Vec3(endMat[3][0], endMat[3][1], endMat[3][2]) - math::Vec3(prevMat[3][0], prevMat[3][1], prevMat[3][2]));
        } else if (m_hasPreviousRootTransformBeforeCancel) {
            deltaPos = math::Vec3(currMat[3][0], currMat[3][1], currMat[3][2])
                     - math::Vec3(prevMat[3][0], prevMat[3][1], prevMat[3][2]);
        }

        if (m_hasPreviousRootTransformBeforeCancel) {
            m_rootMotionDelta.translation = deltaPos;
            m_rootMotionDelta.rotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
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
    } else if (m_stateMachine) {
        // State machine: sample root motion from current state's clip only (not blended pose).
        // This avoids wrong deltas during transitions (blended pose would lerp root toward other state's bind).
        const size_t rmb = static_cast<size_t>(m_rootMotionBoneIndex);
        if (!m_skeleton || rmb >= m_skeleton->BoneCount()) {
            m_rootMotionDelta.hasMotion = false;
            return;
        }
        const int currentState = m_stateMachine->GetCurrentState();
        if (currentState < 0) {
            m_rootMotionDelta.hasMotion = false;
            return;
        }
        auto clip = m_stateMachine->GetStateClip(currentState);
        if (!clip) {
            m_rootMotionDelta.hasMotion = false;
            return;
        }
        const float sampleTime = m_stateMachine->GetStateTime() * m_stateMachine->GetStatePlaybackSpeed(currentState);
        Pose samplePose = BlendSystem::SampleClip(*clip, sampleTime, *m_skeleton);
        if (rmb >= samplePose.GetBoneCount()) {
            m_rootMotionDelta.hasMotion = false;
            return;
        }
        animatedRootTransform = samplePose.GetBoneTransform(rmb);
        math::Mat4 currentRootMat = math::Compose(
            animatedRootTransform.position, animatedRootTransform.rotation, animatedRootTransform.scale);

        if (currentState != m_previousRootMotionState) {
            // First frame in this state: no delta (avoid spike from state change)
            m_previousRootTransformBeforeCancel = currentRootMat;
            m_previousRootMotionState = currentState;
            m_previousRootMotionStateTime = sampleTime;
            m_hasPreviousRootTransformBeforeCancel = true;
            m_rootMotionDelta.translation = math::Vec3(0.0f);
            m_rootMotionDelta.rotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
            m_rootMotionDelta.hasMotion = false;
            return;
        }

        // Same state: use position difference (straight displacement, no rotation contaminating translation)
        Pose prevPose = BlendSystem::SampleClip(*clip, m_previousRootMotionStateTime, *m_skeleton);
        if (rmb >= prevPose.GetBoneCount()) {
            m_previousRootTransformBeforeCancel = currentRootMat;
            m_previousRootMotionStateTime = sampleTime;
            m_rootMotionDelta.hasMotion = false;
            return;
        }
        math::Transform prevRootT = prevPose.GetBoneTransform(rmb);
        math::Mat4 previousRootMat = math::Compose(prevRootT.position, prevRootT.rotation, prevRootT.scale);

        math::Vec3 deltaPos = math::Vec3(currentRootMat[3][0], currentRootMat[3][1], currentRootMat[3][2])
                            - math::Vec3(previousRootMat[3][0], previousRootMat[3][1], previousRootMat[3][2]);
        math::Quat deltaRot = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);  // rotation not used for straight movement

        m_rootMotionDelta.translation = deltaPos;
        m_rootMotionDelta.rotation = deltaRot;
        m_rootMotionDelta.hasMotion = true;
        m_previousRootTransformBeforeCancel = currentRootMat;
        m_previousRootMotionStateTime = sampleTime;
        return;
    } else {
        // Blend tree: read root from pose (no state machine)
        if (m_pose.GetBoneCount() <= static_cast<size_t>(m_rootMotionBoneIndex)) {
            m_rootMotionDelta.hasMotion = false;
            return;
        }
        animatedRootTransform = m_pose.GetBoneTransform(static_cast<size_t>(m_rootMotionBoneIndex));
    }
    
    // Shared path (single-clip already returned): get current root transform in local space (WITH root motion)
    math::Mat4 currentRootMat = math::Compose(
        animatedRootTransform.position, animatedRootTransform.rotation, animatedRootTransform.scale);
    
    // Calculate delta from previous frame: use position difference (straight displacement)
    if (m_hasPreviousRootTransformBeforeCancel) {
        math::Vec3 deltaPos = math::Vec3(currentRootMat[3][0], currentRootMat[3][1], currentRootMat[3][2])
                            - math::Vec3(m_previousRootTransformBeforeCancel[3][0], m_previousRootTransformBeforeCancel[3][1], m_previousRootTransformBeforeCancel[3][2]);
        m_rootMotionDelta.translation = deltaPos;
        m_rootMotionDelta.rotation = math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
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
    if (m_globalBoneMatrices.size() != count) m_globalBoneMatrices.assign(count, math::Mat4(1.0f));

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
        
        // CRITICAL: Root motion bone handling
        if (m_rootMotionEnabled) {
            // CRITICAL: Always neutralize the actual root bone (parentIndex == -1) completely
            // This is usually a dummy root node that has no spatial meaning
            if (b.parentIndex == -1) {
                global[idx] = math::Mat4(1.0f); // Force identity
                computed[idx] = 1;
                return;
            }
            // Root motion bone: Use its local transform (which has bind pose translation)
            // This maintains skeleton hierarchy - the bone keeps its bind pose position relative to root
            // Translation delta is extracted separately for root motion
            if (static_cast<int>(idx) == m_rootMotionBoneIndex && m_rootMotionBoneIndex >= 0) {
                // Root motion bone uses its local transform (bind pose translation + animated rotation/scale)
                // Don't force identity - let it compute normally from local transform
                // The parent (actual root) is identity, so this becomes: identity * local = local
                // This maintains skeleton integrity
            }
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
        m_globalBoneMatrices[i] = global[i];

        // CRITICAL: Root motion bone final matrix must be identity-based
        // RULE: Root motion lives ONLY in world/entity space.
        // RULE: Root bone is always visual identity when root motion is enabled.
        // RULE: Skeleton root is virtual and has no spatial meaning.
        if (m_rootMotionEnabled) {
            // CRITICAL: Always neutralize the actual root bone (parentIndex == -1) completely
            // This is usually a dummy root node that has no spatial meaning
            if (b.parentIndex == -1) {
                // Actual root bone: completely neutral - pure identity matrix
                m_boneMatrices[i] = math::Mat4(1.0f);
            }
            // Root motion bone: Use standard skinning formula with its bind pose translation
            // This maintains skeleton hierarchy - the bone keeps its proper transform for skinning
            // Translation delta is extracted separately for root motion
            else if (static_cast<int>(i) == m_rootMotionBoneIndex && m_rootMotionBoneIndex >= 0) {
                // Root motion bone uses standard formula - maintains skeleton integrity
                // The bone has bind pose translation + animated rotation/scale
                // This ensures proper skinning while root motion is extracted from translation delta
                m_boneMatrices[i] = m_skeleton->GetGlobalInverseTransform() * global[i] * b.inverseBindMatrix;
            } else {
                m_boneMatrices[i] = m_skeleton->GetGlobalInverseTransform() * global[i] * b.inverseBindMatrix;
            }
        } else {
            m_boneMatrices[i] = m_skeleton->GetGlobalInverseTransform() * global[i] * b.inverseBindMatrix;
        }
    }
}

} // namespace animation
} // namespace eyelib
