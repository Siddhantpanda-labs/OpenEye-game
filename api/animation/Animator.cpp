#include "Animator.h"

#include "eyelib/animation/Animator.h"
#include "eyelib/animation/Skeleton.h"

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
    if (!m_impl->rootMotionBoneName.empty()) {
        SetRootMotionBone(m_impl->rootMotionBoneName);
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
    
    if (!boneName.empty() && m_impl->skeleton) {
        int boneIndex = m_impl->skeleton->FindBoneIndex(boneName);
        if (boneIndex < 0) {
            // Try common root bone names
            const std::vector<std::string> commonNames = {"root", "Root", "Armature", "Hips", "hips"};
            for (const auto& name : commonNames) {
                boneIndex = m_impl->skeleton->FindBoneIndex(name);
                if (boneIndex >= 0) {
                    m_impl->rootMotionBoneName = name;
                    break;
                }
            }
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
