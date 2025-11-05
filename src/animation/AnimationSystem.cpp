#include "AnimationSystem.h"
#include "AnimationStateMachine.h"
#include "ecs/Components.h"
#include "core/log/Log.h"
#include <algorithm>
#include <functional>
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
    
    if (!animComponent.skeleton) {
        return;
    }
    
    // Update state machine if present (this drives animation selection)
    if (animComponent.stateMachine) {
        auto stateMachine = std::static_pointer_cast<AnimationStateMachine>(animComponent.stateMachine);
        if (stateMachine && stateMachine->IsValid()) {
            // Update state machine (evaluates transitions)
            stateMachine->Update(deltaTime);
            
            // Get current state's clip index and settings
            int newClipIndex = stateMachine->GetCurrentClipIndex();
            float newPlaybackSpeed = stateMachine->GetCurrentPlaybackSpeed();
            bool newLoop = stateMachine->GetCurrentLoop();
            float blendTime = stateMachine->GetCurrentBlendTime();
            
            // If state machine changed the clip, update animation
            if (newClipIndex >= 0 && newClipIndex != animComponent.currentClipIndex) {
                PlayAnimation(registry, entity, newClipIndex, blendTime);
                animComponent.playbackSpeed = newPlaybackSpeed;
                animComponent.loop = newLoop;
            } else if (newClipIndex >= 0) {
                // Update playback speed and loop from state machine
                animComponent.playbackSpeed = newPlaybackSpeed;
                animComponent.loop = newLoop;
            }
        }
    }
    
    // Update animation time (only if playing and has a clip)
    if (animComponent.isPlaying && animComponent.currentClipIndex >= 0 && 
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
            
            // Mark matrices as dirty when time changes
            animComponent.boneMatricesDirty = true;
        }
    }
    
    // Update blend time
    if (animComponent.blendFromClipIndex >= 0) {
        animComponent.blendTime += deltaTime;
        if (animComponent.blendTime >= animComponent.blendDuration) {
            animComponent.blendFromClipIndex = -1;
            animComponent.blendTime = 0.0f;
        }
        animComponent.boneMatricesDirty = true;
    }
    
    // Always calculate bone matrices (even if paused/stopped, to show current pose)
    // This ensures the model is rendered with correct pose regardless of playback state
    // Force calculation every frame to ensure animation is applied
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
    
    // Sample current animation (even if not playing, use bind pose or current time)
    std::vector<glm::mat4> localTransforms(boneCount, glm::mat4(1.0f));
    SampleAnimation(animComponent, localTransforms);
    
    // Build final bone matrices from skeleton hierarchy
    BuildBoneMatrices(*skeleton, localTransforms, animComponent.finalBoneMatrices);
    
    // Debug: Verify matrices are being calculated
    if (animComponent.finalBoneMatrices.empty()) {
        OE_WARN_F("CalculateBoneMatrices: finalBoneMatrices is empty! boneCount=%zu", boneCount);
    }
    
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
    if (!skeleton) {
        return;
    }
    
    size_t boneCount = skeleton->GetBoneCount();
    if (boneCount == 0) {
        return;
    }
    
    // Initialize all transforms to bind pose (fallback)
    for (size_t i = 0; i < boneCount; ++i) {
        const auto& bone = skeleton->GetBone(i);
        outLocalTransforms[i] = bone.GetBindLocalMatrix();
    }
    
    if (animComponent.currentClipIndex < 0) {
        // No animation selected, use bind pose (already set above)
        return;
    }
    
    if (animComponent.currentClipIndex >= static_cast<int>(animComponent.clips.size())) {
        return;
    }
    
    auto currentClip = std::static_pointer_cast<AnimationClip>(animComponent.clips[animComponent.currentClipIndex]);
    if (!currentClip) {
        return;
    }
    
    // Build bone index list for sampling (boneCount already defined above at function start)
    // This is just a sequential mapping [0, 1, 2, ..., boneCount-1]
    // The animation channels already have boneIndex set to skeleton indices
    std::vector<int> boneIndices(boneCount);
    for (size_t i = 0; i < boneCount; ++i) {
        boneIndices[i] = static_cast<int>(i);
    }
    
    // Sample current animation (channels will directly index into outLocalTransforms)
    currentClip->Sample(animComponent.currentTime, outLocalTransforms, boneIndices);
    
    // Debug: Check if animation actually modified transforms (not just bind pose)
    static float lastDebugTime = 0.0f;
    float currentTime = animComponent.currentTime;
    if (std::abs(currentTime - lastDebugTime) > 0.5f) { // Log every 0.5 seconds
        size_t animatedBoneCount = 0;
        size_t totalAnimatedBoneCount = 0;
        size_t checkedCount = std::min(boneCount, static_cast<size_t>(50)); // Check first 50 bones
        for (size_t i = 0; i < checkedCount; i++) {
            const auto& bone = skeleton->GetBone(i);
            if (outLocalTransforms[i] != bone.GetBindLocalMatrix()) {
                animatedBoneCount++;
            }
        }
        // Count total animated bones (check all bones)
        for (size_t i = 0; i < boneCount; i++) {
            const auto& bone = skeleton->GetBone(i);
            if (outLocalTransforms[i] != bone.GetBindLocalMatrix()) {
                totalAnimatedBoneCount++;
            }
        }
        OE_INFO_F("Animation sampling: time=%.2f/%.2f, clip='%s', animatedBones=%zu/%zu (first 50), totalAnimated=%zu/%zu, channels=%zu",
                 animComponent.currentTime, currentClip->GetDuration(), currentClip->GetName().c_str(),
                 animatedBoneCount, checkedCount, totalAnimatedBoneCount, boneCount, currentClip->GetChannelCount());
        lastDebugTime = currentTime;
    }
    
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
    // We need to process bones in hierarchical order (parents before children)
    // Use a recursive approach: process roots first, then children
    std::vector<glm::mat4> worldTransforms(boneCount);
    
    // Helper lambda to recursively build world transforms
    std::function<void(int, glm::mat4)> buildWorldTransform = [&](int boneIndex, glm::mat4 parentWorld) {
        if (boneIndex < 0 || boneIndex >= static_cast<int>(boneCount)) {
            return;
        }
        
        const auto& bone = skeleton.GetBone(boneIndex);
        glm::mat4 localTransform = localTransforms[boneIndex];
        
        // Calculate world transform: parent world * local
        glm::mat4 worldTransform = parentWorld * localTransform;
        worldTransforms[boneIndex] = worldTransform;
        
        // Process all children (find children by checking parentIndex)
        for (size_t i = 0; i < boneCount; ++i) {
            const auto& childBone = skeleton.GetBone(i);
            if (childBone.parentIndex == boneIndex) {
                buildWorldTransform(static_cast<int>(i), worldTransform);
            }
        }
    };
    
    // Find all root bones (parentIndex == -1) and process them
    for (size_t i = 0; i < boneCount; ++i) {
        const auto& bone = skeleton.GetBone(i);
        if (bone.parentIndex < 0) {
            // Root bone: world = local
            glm::mat4 localTransform = localTransforms[i];
            worldTransforms[i] = localTransform;
            
            // Process children recursively
            for (size_t j = 0; j < boneCount; ++j) {
                const auto& childBone = skeleton.GetBone(j);
                if (childBone.parentIndex == static_cast<int>(i)) {
                    buildWorldTransform(static_cast<int>(j), localTransform);
                }
            }
        }
    }
    
    // Second pass: multiply by inverse bind matrix for skinning
    // Standard skinning formula: finalMatrix = currentTransform * inverseBindMatrix
    // IMPORTANT: Bone matrices should be in MODEL SPACE, not world space
    // The shader will apply the model matrix separately (u_Model)
    // So bone matrices transform from mesh bind pose space to animated MODEL space
    // The inverse bind matrix transforms from mesh bind pose space to bone bind pose space
    // The world transform (in model space) transforms from bone space to animated model space
    const glm::mat4& globalInverse = skeleton.GetGlobalInverseTransform();
    for (size_t i = 0; i < boneCount; ++i) {
        const auto& bone = skeleton.GetBone(i);
        // Multiply with global inverse to bring scene/world to model space, then apply offset
        outFinalMatrices[i] = globalInverse * worldTransforms[i] * bone.inverseBindMatrix;
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

