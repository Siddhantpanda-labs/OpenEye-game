#pragma once

#include "api/animation/All.h"
#include "api/assets/All.h"
#include "api/world/All.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace openeye::player {

class WorldAnimationBridge {
public:
    void BuildFromWorld(const eyelib::api::world::World& world, eyelib::api::AssetRegistry& registry);
    void Update(float deltaSeconds);
    const std::vector<eyelib::math::Mat4>* GetBoneMatrices(::eyelib::world::EntityId entity) const;
    bool SetFloat(::eyelib::world::EntityId entity, const std::string& name, float value);
    bool SetBool(::eyelib::world::EntityId entity, const std::string& name, bool value);
    bool SetTrigger(::eyelib::world::EntityId entity, const std::string& name);

private:
    struct AnimatorBinding {
        ::eyelib::world::EntityId entity;
        std::shared_ptr<eyelib::api::Animator> animator;
    };
    struct BoneEntry {
        ::eyelib::world::EntityId entity;
        std::vector<eyelib::math::Mat4> matrices;
    };

    std::shared_ptr<eyelib::api::AnimationClip> LoadClip(eyelib::api::AssetRegistry& registry, const std::string& path);
    std::shared_ptr<eyelib::api::Skeleton> LoadSkeleton(eyelib::api::AssetRegistry& registry, const std::string& path);

    std::unordered_map<std::string, std::shared_ptr<eyelib::api::AnimationClip>> m_clipCache;
    std::unordered_map<std::string, std::shared_ptr<eyelib::api::Skeleton>> m_skeletonCache;
    std::unordered_map<::eyelib::world::EntityId, AnimatorBinding, ::eyelib::world::EntityIdHash> m_animators;
    std::vector<BoneEntry> m_bones;
};

} // namespace openeye::player
