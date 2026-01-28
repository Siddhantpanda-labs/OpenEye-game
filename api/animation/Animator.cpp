#include "Animator.h"

#include "eyelib/animation/Animator.h"
#include "eyelib/animation/Skeleton.h"
#include "api/core/Log.h"
#include <algorithm>
#include <cctype>

namespace eyelib {
namespace api {

class Animator::Impl {
public:
    std::shared_ptr<eyelib::animation::Animator> animator = std::make_shared<eyelib::animation::Animator>();
    std::vector<eyelib::math::Mat4> boneMatrices;
    
    // Root motion tracking (API layer only stores configuration and results)
    std::shared_ptr<eyelib::animation::Skeleton> skeleton; // Store skeleton reference
    std::string rootMotionBoneName;
    bool rootMotionEnabled = false;
    MotionAuthority motionAuthority = MotionAuthority::ControllerDriven;
    RootMotionDelta rootMotionDelta; // Cached from library
};

Animator::Animator() : m_impl(std::make_unique<Impl>()) {}
Animator::~Animator() = default;

void Animator::SetSkeleton(std::shared_ptr<Skeleton> skeleton) {
    if (!skeleton) return;
    m_impl->skeleton = skeleton->GetInternal();
    m_impl->animator->SetSkeleton(m_impl->skeleton);
    
    // Reset root motion tracking when skeleton changes
    // If root motion was enabled, re-apply it to find the root bone
    if (!m_impl->rootMotionBoneName.empty() || m_impl->rootMotionEnabled) {
        SetRootMotionBone(m_impl->rootMotionBoneName.empty() ? "" : m_impl->rootMotionBoneName);
    }
}

void Animator::SetClip(std::shared_ptr<AnimationClip> clip) {
    if (!clip) return;
    m_impl->animator->SetClip(clip->GetInternal());
}

void Animator::SetStateMachine(std::shared_ptr<StateMachine> sm) {
    if (!sm) return;
    m_impl->animator->SetStateMachine(sm->GetInternal());
}

void Animator::SetBlendTree(std::shared_ptr<BlendTree> tree) {
    if (!tree) return;
    m_impl->animator->SetBlendTree(tree->GetInternal());
}

void Animator::Update(float deltaTime) {
    // Delegate to library implementation - root motion extraction happens there
    m_impl->animator->Update(deltaTime);
    
    // Copy root motion delta from library
    if (m_impl->rootMotionEnabled) {
        auto libDelta = m_impl->animator->GetRootMotionDelta();
        m_impl->rootMotionDelta.translation = libDelta.translation;
        m_impl->rootMotionDelta.rotation = libDelta.rotation;
        m_impl->rootMotionDelta.hasMotion = libDelta.hasMotion;
    } else {
        m_impl->rootMotionDelta.hasMotion = false;
    }
    
    // Copy bone matrices from library (already computed with root motion canceled)
    m_impl->boneMatrices = m_impl->animator->GetBoneMatrices();
}

const std::vector<eyelib::math::Mat4>& Animator::GetBoneMatrices() const {
    return m_impl->boneMatrices;
}

void Animator::SetParameter(const std::string& name, float value) {
    m_impl->animator->SetParameter(name, value);
}

void Animator::SetParameter(const std::string& name, bool value) {
    m_impl->animator->SetParameter(name, value);
}

void Animator::SetTrigger(const std::string& name) {
    m_impl->animator->SetTrigger(name);
}

std::shared_ptr<Animator> Animator::WrapInternal(const std::shared_ptr<eyelib::animation::Animator>& animator) {
    auto apiAnimator = std::make_shared<Animator>();
    if (animator) apiAnimator->m_impl->animator = animator;
    return apiAnimator;
}

void Animator::SetRootMotionBone(const std::string& boneName) {
    m_impl->rootMotionBoneName = boneName;
    
    if (m_impl->skeleton) {
        int boneIndex = -1;
        
        if (!boneName.empty()) {
            boneIndex = m_impl->skeleton->FindBoneIndex(boneName);
            if (boneIndex < 0) {
                // Try common root bone names (prioritize Hips/hips as they're usually the actual root motion bone)
                const std::vector<std::string> commonNames = {"Hips", "hips", "mixamorig:Hips", "root", "Root", "Armature"};
                for (const auto& name : commonNames) {
                    boneIndex = m_impl->skeleton->FindBoneIndex(name);
                    if (boneIndex >= 0) {
                        m_impl->rootMotionBoneName = name;
                        Log::InfoF("DEBUG: Found root motion bone by name: %s (index %d)", name.c_str(), boneIndex);
                        break;
                    }
                }
            }
        }
        
        // Fallback: Find root motion bone
        // Priority 1: Find bone named "Hips" or similar (usually the actual root motion bone)
        // Priority 2: Find first child of root bone (parentIndex == root bone index) that has position animation
        // Priority 3: Find actual root bone (parentIndex == -1)
        if (boneIndex < 0) {
            const size_t boneCount = m_impl->skeleton->BoneCount();
            
            // First, find the actual root bone (parentIndex == -1)
            int actualRootIndex = -1;
            for (size_t i = 0; i < boneCount; ++i) {
                const auto& bone = m_impl->skeleton->GetBone(i);
                if (bone.parentIndex == -1) {
                    actualRootIndex = static_cast<int>(i);
                    Log::InfoF("DEBUG: Found actual root bone: index %d, name: %s", actualRootIndex, bone.name.c_str());
                    break;
                }
            }
            
            // Priority: Find first child of root that has "Hips" in name or is a direct child
            if (actualRootIndex >= 0) {
                for (size_t i = 0; i < boneCount; ++i) {
                    const auto& bone = m_impl->skeleton->GetBone(i);
                    if (bone.parentIndex == actualRootIndex) {
                        // Check if it's a hips bone or similar
                        std::string lowerName = bone.name;
                        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                        if (lowerName.find("hip") != std::string::npos || 
                            lowerName.find("pelvis") != std::string::npos ||
                            i == 0) { // Often bone 0 is the hips
                            boneIndex = static_cast<int>(i);
                            m_impl->rootMotionBoneName = bone.name;
                            Log::InfoF("DEBUG: Found root motion bone (child of root): index %d, name: %s", 
                                boneIndex, bone.name.c_str());
                            break;
                        }
                    }
                }
            }
            
            // Final fallback: Use actual root bone
            if (boneIndex < 0 && actualRootIndex >= 0) {
                boneIndex = actualRootIndex;
                const auto& bone = m_impl->skeleton->GetBone(static_cast<size_t>(boneIndex));
                m_impl->rootMotionBoneName = bone.name;
                Log::WarnF("DEBUG: Using actual root bone as root motion bone: index %d, name: %s", 
                    boneIndex, bone.name.c_str());
            }
            
            if (boneIndex < 0) {
                Log::Warn("DEBUG: No root motion bone found!");
            }
        }
        
        // Final fallback: Use bone 0 if no root bone found (safety net)
        if (boneIndex < 0 && m_impl->skeleton->BoneCount() > 0) {
            boneIndex = 0;
            const auto& bone = m_impl->skeleton->GetBone(0);
            m_impl->rootMotionBoneName = bone.name.empty() ? "Bone0" : bone.name;
            Log::WarnF("DEBUG: Root bone not found by name, using bone 0 as fallback (name: %s, parentIndex: %d)", 
                bone.name.c_str(), bone.parentIndex);
        }
        
        // DEBUG: Log final root bone detection result
        if (boneIndex >= 0) {
            const auto& finalBone = m_impl->skeleton->GetBone(static_cast<size_t>(boneIndex));
            Log::InfoF("DEBUG: Final root motion bone: index %d, name: %s, parentIndex: %d", 
                boneIndex, finalBone.name.c_str(), finalBone.parentIndex);
        } else {
            Log::Error("DEBUG: FAILED to find root motion bone! Index is -1");
        }
        
        // Delegate to library
        m_impl->animator->SetRootMotionBone(boneIndex);
    } else {
        m_impl->animator->SetRootMotionBone(-1);
    }
}

Animator::RootMotionDelta Animator::GetRootMotionDelta() const {
    return m_impl->rootMotionDelta;
}

void Animator::SetRootMotionEnabled(bool enabled) {
    m_impl->rootMotionEnabled = enabled;
    // Delegate to library
    m_impl->animator->SetRootMotionEnabled(enabled);
    
    // If enabling root motion and skeleton exists, ensure root bone is found
    if (enabled && m_impl->skeleton) {
        // Re-apply root motion bone detection to ensure root bone is found
        SetRootMotionBone(m_impl->rootMotionBoneName.empty() ? "" : m_impl->rootMotionBoneName);
    }
    
    if (!enabled) {
        m_impl->rootMotionDelta.hasMotion = false;
    }
}

void Animator::SetMotionAuthority(MotionAuthority authority) {
    m_impl->motionAuthority = authority;
}

Animator::MotionAuthority Animator::GetMotionAuthority() const {
    return m_impl->motionAuthority;
}

std::shared_ptr<eyelib::animation::Animator> Animator::GetInternal() const {
    return m_impl->animator;
}

} // namespace api
} // namespace eyelib
