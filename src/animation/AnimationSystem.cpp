#include "AnimationSystem.h"
#include "ecs/Components.h"
#include "core/log/Log.h"
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

namespace OpenEye {
namespace Animation {

void AnimationSystem::Update(ECS::Scene& scene, float deltaTime) {
    auto& registry = scene.GetRegistry();
    auto view = registry.view<ECS::AnimationComponent>();
    
    for (auto entity : view) {
        UpdateEntity(registry, entity, deltaTime);
    }
}

void AnimationSystem::UpdateEntity(entt::registry& registry, entt::entity entity, float deltaTime) {
    auto& animComponent = registry.get<ECS::AnimationComponent>(entity);
    
    if (!animComponent.skeleton || animComponent.currentClipIndex < 0) {
        return;
    }
    
    if (!animComponent.isPlaying) {
        return;
    }
    
    // Update animation time
    if (animComponent.currentClipIndex >= 0 && 
        animComponent.currentClipIndex < static_cast<int>(animComponent.clips.size())) {
        
        auto clip = std::static_pointer_cast<AnimationClip>(animComponent.clips[animComponent.currentClipIndex]);
        if (clip) {
            animComponent.currentTime += deltaTime * animComponent.playbackSpeed;
            
            // Handle looping
            float duration = clip->GetDuration();
            if (animComponent.loop && animComponent.currentTime >= duration) {
                animComponent.currentTime = std::fmod(animComponent.currentTime, duration);
            } else if (!animComponent.loop && animComponent.currentTime >= duration) {
                animComponent.currentTime = duration;
                animComponent.isPlaying = false;
            }
        }
    }
    
    // Update blend time
    if (animComponent.blendFromClipIndex >= 0) {
        animComponent.blendTime += deltaTime;
        if (animComponent.blendTime >= animComponent.blendDuration) {
            animComponent.blendFromClipIndex = -1;
            animComponent.blendTime = 0.0f;
        }
    }
    
    // Calculate bone matrices
    CalculateBoneMatrices(registry, entity);
}

void AnimationSystem::CalculateBoneMatrices(entt::registry& registry, entt::entity entity) {
    auto& animComponent = registry.get<ECS::AnimationComponent>(entity);
    
    if (!animComponent.skeleton) {
        return;
    }
    
    auto skeleton = std::static_pointer_cast<Skeleton>(animComponent.skeleton);
    if (!skeleton) {
        return;
    }
    
    size_t boneCount = skeleton->GetBoneCount();
    if (boneCount == 0) {
        return;
    }
    
    // Ensure final bone matrices array is correct size
    if (animComponent.finalBoneMatrices.size() != boneCount) {
        animComponent.finalBoneMatrices.resize(boneCount, glm::mat4(1.0f));
    }
    
    // Sample current animation
    std::vector<glm::mat4> localTransforms(boneCount, glm::mat4(1.0f));
    SampleAnimation(animComponent, localTransforms);
    
    // Build final bone matrices from skeleton hierarchy
    BuildBoneMatrices(*skeleton, localTransforms, animComponent.finalBoneMatrices);
    
    animComponent.boneMatricesDirty = false;
}

void AnimationSystem::PlayAnimation(entt::registry& registry, entt::entity entity,
                                   int clipIndex, float blendTime) {
    auto& animComponent = registry.get<ECS::AnimationComponent>(entity);
    
    if (clipIndex < 0 || clipIndex >= static_cast<int>(animComponent.clips.size())) {
        OE_WARN_F("Invalid animation clip index: %d", clipIndex);
        return;
    }
    
    // Start blending from current clip
    if (animComponent.currentClipIndex >= 0 && blendTime > 0.0f) {
        animComponent.blendFromClipIndex = animComponent.currentClipIndex;
        animComponent.blendTime = 0.0f;
        animComponent.blendDuration = blendTime;
    } else {
        animComponent.blendFromClipIndex = -1;
        animComponent.blendTime = 0.0f;
    }
    
    animComponent.currentClipIndex = clipIndex;
    animComponent.currentTime = 0.0f;
    animComponent.isPlaying = true;
    animComponent.boneMatricesDirty = true;
}

void AnimationSystem::StopAnimation(entt::registry& registry, entt::entity entity) {
    auto& animComponent = registry.get<ECS::AnimationComponent>(entity);
    animComponent.isPlaying = false;
    animComponent.boneMatricesDirty = true;
}

void AnimationSystem::SetPlaybackSpeed(entt::registry& registry, entt::entity entity, float speed) {
    auto& animComponent = registry.get<ECS::AnimationComponent>(entity);
    animComponent.playbackSpeed = std::max(0.0f, speed);
}

void AnimationSystem::SampleAnimation(ECS::AnimationComponent& animComponent,
                                      std::vector<glm::mat4>& outLocalTransforms) {
    auto skeleton = std::static_pointer_cast<Skeleton>(animComponent.skeleton);
    if (!skeleton || animComponent.currentClipIndex < 0) {
        // No animation, use bind pose
        size_t boneCount = skeleton ? skeleton->GetBoneCount() : 0;
        for (size_t i = 0; i < boneCount; ++i) {
            const auto& bone = skeleton->GetBone(i);
            outLocalTransforms[i] = bone.GetBindLocalMatrix();
        }
        return;
    }
    
    if (animComponent.currentClipIndex >= static_cast<int>(animComponent.clips.size())) {
        return;
    }
    
    auto currentClip = std::static_pointer_cast<AnimationClip>(animComponent.clips[animComponent.currentClipIndex]);
    if (!currentClip) {
        return;
    }
    
    // Build bone index list for sampling
    size_t boneCount = skeleton->GetBoneCount();
    std::vector<int> boneIndices(boneCount);
    for (size_t i = 0; i < boneCount; ++i) {
        boneIndices[i] = static_cast<int>(i);
    }
    
    // Sample current animation
    currentClip->Sample(animComponent.currentTime, outLocalTransforms, boneIndices);
    
    // If blending, blend with previous animation
    if (animComponent.blendFromClipIndex >= 0 &&
        animComponent.blendFromClipIndex < static_cast<int>(animComponent.clips.size())) {
        
        auto blendFromClip = std::static_pointer_cast<AnimationClip>(animComponent.clips[animComponent.blendFromClipIndex]);
        if (!blendFromClip) {
            return;
        }
        
        std::vector<glm::mat4> blendFromTransforms(boneCount, glm::mat4(1.0f));
        blendFromClip->Sample(animComponent.currentTime, blendFromTransforms, boneIndices);
        
        // Blend factor (0 = fully from, 1 = fully to)
        float blendFactor = animComponent.blendDuration > 0.0f
            ? std::min(1.0f, animComponent.blendTime / animComponent.blendDuration)
            : 1.0f;
        
        BlendAnimations(blendFromTransforms, outLocalTransforms, blendFactor, outLocalTransforms);
    }
    
    // Fill in bones not animated (use bind pose)
    for (size_t i = 0; i < boneCount; ++i) {
        if (outLocalTransforms[i] == glm::mat4(0.0f)) {
            const auto& bone = skeleton->GetBone(i);
            outLocalTransforms[i] = bone.GetBindLocalMatrix();
        }
    }
}

void AnimationSystem::BuildBoneMatrices(const Skeleton& skeleton,
                                       const std::vector<glm::mat4>& localTransforms,
                                       std::vector<glm::mat4>& outFinalMatrices) {
    if (skeleton.GetBoneCount() == 0) {
        return;
    }
    
    size_t boneCount = skeleton.GetBoneCount();
    
    // First pass: build world-space transforms by traversing hierarchy
    std::vector<glm::mat4> worldTransforms(boneCount);
    
    for (size_t i = 0; i < boneCount; ++i) {
        const auto& bone = skeleton.GetBone(i);
        glm::mat4 localTransform = localTransforms[i];
        
        if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int>(boneCount)) {
            // Multiply by parent's world transform
            worldTransforms[i] = worldTransforms[bone.parentIndex] * localTransform;
        } else {
            // Root bone
            worldTransforms[i] = localTransform;
        }
    }
    
    // Second pass: multiply by inverse bind matrix for skinning
    for (size_t i = 0; i < boneCount; ++i) {
        const auto& bone = skeleton.GetBone(i);
        outFinalMatrices[i] = worldTransforms[i] * bone.inverseBindMatrix;
    }
}

void AnimationSystem::BlendAnimations(const std::vector<glm::mat4>& from,
                                      const std::vector<glm::mat4>& to,
                                      float t,
                                      std::vector<glm::mat4>& out) {
    if (from.size() != to.size() || out.size() != from.size()) {
        return;
    }
    
    // Blend by decomposing matrices, interpolating, and recomposing
    for (size_t i = 0; i < from.size(); ++i) {
        glm::quat rotationFrom, rotationTo;
        glm::vec3 translationFrom, translationTo;
        // Manual decomposition
        glm::mat4 mFrom = from[i];
        glm::vec3 scaleFrom(
            glm::length(glm::vec3(mFrom[0])),
            glm::length(glm::vec3(mFrom[1])),
            glm::length(glm::vec3(mFrom[2]))
        );
        glm::mat3 rotMatFrom(
            glm::vec3(mFrom[0]) / scaleFrom.x,
            glm::vec3(mFrom[1]) / scaleFrom.y,
            glm::vec3(mFrom[2]) / scaleFrom.z
        );
        rotationFrom = glm::quat_cast(rotMatFrom);
        translationFrom = glm::vec3(mFrom[3]);
        
        glm::mat4 mTo = to[i];
        glm::vec3 scaleTo(
            glm::length(glm::vec3(mTo[0])),
            glm::length(glm::vec3(mTo[1])),
            glm::length(glm::vec3(mTo[2]))
        );
        glm::mat3 rotMatTo(
            glm::vec3(mTo[0]) / scaleTo.x,
            glm::vec3(mTo[1]) / scaleTo.y,
            glm::vec3(mTo[2]) / scaleTo.z
        );
        rotationTo = glm::quat_cast(rotMatTo);
        translationTo = glm::vec3(mTo[3]);
        
        // Interpolate
        glm::vec3 scale = glm::mix(scaleFrom, scaleTo, t);
        glm::quat rotation = glm::slerp(rotationFrom, rotationTo, t);
        glm::vec3 translation = glm::mix(translationFrom, translationTo, t);
        
        // Recompose
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation);
        transform = transform * glm::mat4_cast(rotation);
        transform = glm::scale(transform, scale);
        
        out[i] = transform;
    }
}

} // namespace Animation
} // namespace OpenEye

