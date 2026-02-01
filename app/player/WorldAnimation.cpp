#include "WorldAnimation.h"

#include "api/core/All.h"

namespace openeye::player {

std::shared_ptr<eyelib::api::AnimationClip> WorldAnimationBridge::LoadClip(
    eyelib::api::AssetRegistry& registry, const std::string& path) {
    if (path.empty()) {
        return {};
    }
    auto it = m_clipCache.find(path);
    if (it != m_clipCache.end()) {
        return it->second;
    }
    auto assetId = registry.GetId(path);
    if (!assetId.IsValid()) {
        assetId = registry.LoadAsset(path, eyelib::api::AssetType::Animation);
    }
    auto clip = registry.GetAnimationClip(assetId, 0);
    if (!clip) {
        eyelib::api::core::Log::WarnF("Animation clip unavailable: %s", path.c_str());
    }
    m_clipCache.emplace(path, clip);
    return clip;
}

std::shared_ptr<eyelib::api::Skeleton> WorldAnimationBridge::LoadSkeleton(
    eyelib::api::AssetRegistry& registry, const std::string& path) {
    if (path.empty()) {
        return {};
    }
    auto it = m_skeletonCache.find(path);
    if (it != m_skeletonCache.end()) {
        return it->second;
    }
    auto assetId = registry.GetId(path);
    if (!assetId.IsValid()) {
        assetId = registry.LoadAsset(path, eyelib::api::AssetType::Animation);
    }
    auto skeleton = registry.GetSkeleton(assetId);
    if (!skeleton) {
        eyelib::api::core::Log::WarnF("Skeleton unavailable: %s", path.c_str());
    }
    m_skeletonCache.emplace(path, skeleton);
    return skeleton;
}

void WorldAnimationBridge::BuildFromWorld(const eyelib::api::world::World& world,
                                          eyelib::api::AssetRegistry& registry) {
    m_animators.clear();
    m_bones.clear();
    const auto animators = world.GetAnimators();
    for (const auto& entry : animators) {
        auto animator = std::make_shared<eyelib::api::Animator>();
        auto clip = LoadClip(registry, entry.component.animatorAssetId);
        auto skeleton = LoadSkeleton(registry,
                                     entry.component.skeletonAssetId.empty()
                                         ? entry.component.animatorAssetId
                                         : entry.component.skeletonAssetId);
        if (skeleton) {
            animator->SetSkeleton(skeleton);
        }
        if (clip) {
            animator->SetClip(clip);
        }
        m_animators.emplace(entry.entity, AnimatorBinding{ entry.entity, animator });
        m_bones.push_back(BoneEntry{ entry.entity, {} });
    }
}

void WorldAnimationBridge::Update(float deltaSeconds) {
    for (auto& [entity, binding] : m_animators) {
        (void)entity;
        if (binding.animator) {
            binding.animator->Update(deltaSeconds);
            const auto& matrices = binding.animator->GetBoneMatrices();
            for (auto& entry : m_bones) {
                if (entry.entity == entity) {
                    entry.matrices = matrices;
                    break;
                }
            }
        }
    }
}

const std::vector<eyelib::math::Mat4>* WorldAnimationBridge::GetBoneMatrices(::eyelib::world::EntityId entity) const {
    for (const auto& entry : m_bones) {
        if (entry.entity == entity) {
            return &entry.matrices;
        }
    }
    return nullptr;
}

} // namespace openeye::player
