#pragma once

#include "ecs/Scene.h"
#include "ecs/Components.h"
#include "Skeleton.h"
#include "AnimationClip.h"

namespace OpenEye {
namespace Animation {

/**
 * @brief ECS system for updating animations and calculating bone matrices.
 * 
 * Updates animation playback, samples keyframes, blends animations,
 * and computes final bone matrices for GPU skinning.
 */
class AnimationSystem {
public:
    /**
     * @brief Update all animated entities in the scene.
     * @param scene ECS scene containing entities with AnimationComponent
     * @param deltaTime Time since last frame (in seconds)
     */
    static void Update(ECS::Scene& scene, float deltaTime);
    
    /**
     * @brief Update animation for a specific entity.
     * @param registry ECS registry
     * @param entity Entity with AnimationComponent
     * @param deltaTime Time since last frame
     */
    static void UpdateEntity(entt::registry& registry, entt::entity entity, float deltaTime);
    
    /**
     * @brief Calculate final bone matrices for an entity.
     * @param registry ECS registry
     * @param entity Entity with AnimationComponent
     */
    static void CalculateBoneMatrices(entt::registry& registry, entt::entity entity);
    
    /**
     * @brief Play an animation clip.
     * @param registry ECS registry
     * @param entity Entity with AnimationComponent
     * @param clipIndex Index of clip to play
     * @param blendTime Blend duration for transition (0 = instant)
     */
    static void PlayAnimation(entt::registry& registry, entt::entity entity, 
                              int clipIndex, float blendTime = 0.2f);
    
    /**
     * @brief Stop animation playback.
     * @param registry ECS registry
     * @param entity Entity with AnimationComponent
     */
    static void StopAnimation(entt::registry& registry, entt::entity entity);
    
    /**
     * @brief Set animation playback speed.
     * @param registry ECS registry
     * @param entity Entity with AnimationComponent
     * @param speed Playback speed multiplier (1.0 = normal, 2.0 = double speed)
     */
    static void SetPlaybackSpeed(entt::registry& registry, entt::entity entity, float speed);

private:
    // Helper: Sample animation and build local transform matrices
    static void SampleAnimation(ECS::AnimationComponent& animComponent,
                                std::vector<glm::mat4>& outLocalTransforms);
    
    // Helper: Build final bone matrices from local transforms using skeleton hierarchy
    static void BuildBoneMatrices(const Skeleton& skeleton,
                                  const std::vector<glm::mat4>& localTransforms,
                                  std::vector<glm::mat4>& outFinalMatrices);
    
    // Helper: Blend two animation samples
    static void BlendAnimations(const std::vector<glm::mat4>& from,
                               const std::vector<glm::mat4>& to,
                               float t,
                               std::vector<glm::mat4>& out);
};

} // namespace Animation
} // namespace OpenEye

