#pragma once

#include "Scene.h"
#include "Components.h"
#include <string>
#include <unordered_map>
#include <json.hpp>

namespace OpenEye {
namespace ECS {

/**
 * @brief Scene serializer for saving/loading scenes to/from JSON.
 * 
 * Provides functionality to serialize ECS scenes including:
 * - Entity ID persistence
 * - Component serialization (Transform, Renderable, Light, etc.)
 * - Parent-child relationships
 * - Asset references (meshes, materials, textures)
 */
class SceneSerializer {
public:
    /**
     * @brief Serialize a scene to JSON file.
     * @param scene Scene to serialize
     * @param filepath Path to save JSON file
     * @return true if successful, false otherwise
     */
    static bool Serialize(const Scene& scene, const std::string& filepath);
    
    /**
     * @brief Deserialize a scene from JSON file.
     * @param scene Scene to populate
     * @param filepath Path to load JSON file from
     * @return true if successful, false otherwise
     */
    static bool Deserialize(Scene& scene, const std::string& filepath);
    
    /**
     * @brief Serialize a scene to JSON object (in-memory).
     * @param scene Scene to serialize
     * @return JSON object representing the scene
     */
    static nlohmann::json SerializeToJson(const Scene& scene);
    
    /**
     * @brief Deserialize a scene from JSON object (in-memory).
     * @param scene Scene to populate
     * @param json JSON object representing the scene
     * @return true if successful, false otherwise
     */
    static bool DeserializeFromJson(Scene& scene, const nlohmann::json& json);

public:
    // Component serialization helpers (public for Prefab to use)
    static nlohmann::json SerializeTransform(const Transform& transform, entt::entity entity, const entt::registry& registry);
    static nlohmann::json SerializeName(const Name& name);
    static nlohmann::json SerializeRenderable(const Renderable& renderable);
    static nlohmann::json SerializeCamera(const CameraComponent& camera);
    static nlohmann::json SerializeLight(const Light& light);
    static nlohmann::json SerializeAABB(const AABB& aabb);
    static nlohmann::json SerializeAnimation(const AnimationComponent& anim);
    static nlohmann::json SerializeAnimator(const AnimatorComponent& animator);
    
    // Component deserialization helpers (public for Prefab to use)
    static bool DeserializeTransform(Transform& transform, const nlohmann::json& json, const entt::registry& registry);
    static bool DeserializeName(Name& name, const nlohmann::json& json);
    static bool DeserializeRenderable(Renderable& renderable, const nlohmann::json& json);
    static bool DeserializeCamera(CameraComponent& camera, const nlohmann::json& json);
    static bool DeserializeLight(Light& light, const nlohmann::json& json);
    static bool DeserializeAABB(AABB& aabb, const nlohmann::json& json);
    static bool DeserializeAnimation(AnimationComponent& anim, const nlohmann::json& json);
    static bool DeserializeAnimator(AnimatorComponent& animator, const nlohmann::json& json);
    
};

} // namespace ECS
} // namespace OpenEye

