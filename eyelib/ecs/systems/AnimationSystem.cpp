#include "AnimationSystem.h"

#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/Animator.h"
#include "eyelib/animation/Animator.h"

namespace eyelib {
namespace ecs {

void AnimationSystem::Update(World& world, float deltaTime) {
    auto view = world.Registry().view<components::Animator>();
    for (auto entity : view) {
        auto& animator = view.get<components::Animator>(entity);
        if (!animator.animator) continue;
        animator.animator->Update(deltaTime);
        animator.boneMatrices = animator.animator->GetBoneMatrices();
    }
}

} // namespace ecs
} // namespace eyelib
