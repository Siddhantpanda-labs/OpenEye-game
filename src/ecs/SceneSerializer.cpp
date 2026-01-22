#include "SceneSerializer.h"
#include "core/log/Log.h"
#include <fstream>
#include <unordered_map>
#include <glm/gtc/quaternion.hpp>

namespace OpenEye {
namespace ECS {

bool SceneSerializer::Serialize(const Scene& scene, const std::string& filepath) {
    try {
        nlohmann::json json = SerializeToJson(scene);
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            OE_ERROR_F("SceneSerializer: Failed to open file for writing: %s", filepath.c_str());
            return false;
        }
        
        file << json.dump(2); // Pretty print with 2-space indent
        file.close();
        
        OE_INFO_F("SceneSerializer: Successfully saved scene to %s", filepath.c_str());
        return true;
    } catch (const std::exception& e) {
        OE_ERROR_F("SceneSerializer: Exception during serialization: %s", e.what());
        return false;
    }
}

bool SceneSerializer::Deserialize(Scene& scene, const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            OE_ERROR_F("SceneSerializer: Failed to open file for reading: %s", filepath.c_str());
            return false;
        }
        
        nlohmann::json json;
        file >> json;
        file.close();
        
        bool result = DeserializeFromJson(scene, json);
        if (result) {
            OE_INFO_F("SceneSerializer: Successfully loaded scene from %s", filepath.c_str());
        }
        return result;
    } catch (const std::exception& e) {
        OE_ERROR_F("SceneSerializer: Exception during deserialization: %s", e.what());
        return false;
    }
}

nlohmann::json SceneSerializer::SerializeToJson(const Scene& scene) {
    nlohmann::json json;
    json["version"] = 1;
    json["entities"] = nlohmann::json::array();
    
    auto& registry = scene.GetRegistry();
    
    // Serialize all entities with their components
    auto view = registry.view<Name>();
    for (auto entity : view) {
        nlohmann::json entityJson;
        entityJson["id"] = static_cast<uint32_t>(entity);
        entityJson["components"] = nlohmann::json::object();
        
        // Serialize Name
        if (registry.all_of<Name>(entity)) {
            entityJson["components"]["Name"] = SerializeName(registry.get<Name>(entity));
        }
        
        // Serialize Transform
        if (registry.all_of<Transform>(entity)) {
            entityJson["components"]["Transform"] = SerializeTransform(
                registry.get<Transform>(entity), entity, registry);
        }
        
        // Serialize Renderable
        if (registry.all_of<Renderable>(entity)) {
            entityJson["components"]["Renderable"] = SerializeRenderable(registry.get<Renderable>(entity));
        }
        
        // Serialize Camera
        if (registry.all_of<CameraComponent>(entity)) {
            entityJson["components"]["Camera"] = SerializeCamera(registry.get<CameraComponent>(entity));
        }
        
        // Serialize Light
        if (registry.all_of<Light>(entity)) {
            entityJson["components"]["Light"] = SerializeLight(registry.get<Light>(entity));
        }
        
        // Serialize AABB
        if (registry.all_of<AABB>(entity)) {
            entityJson["components"]["AABB"] = SerializeAABB(registry.get<AABB>(entity));
        }
        
        // Serialize Animation
        if (registry.all_of<AnimationComponent>(entity)) {
            entityJson["components"]["Animation"] = SerializeAnimation(registry.get<AnimationComponent>(entity));
        }

        // Serialize Animator
        if (registry.all_of<AnimatorComponent>(entity)) {
            entityJson["components"]["Animator"] = SerializeAnimator(registry.get<AnimatorComponent>(entity));
        }
        
        json["entities"].push_back(entityJson);
    }
    
    return json;
}

bool SceneSerializer::DeserializeFromJson(Scene& scene, const nlohmann::json& json) {
    if (!json.contains("version") || !json.contains("entities")) {
        OE_ERROR("SceneSerializer: Invalid JSON format - missing 'version' or 'entities'");
        return false;
    }
    
    auto& registry = scene.GetRegistry();
    
    // First pass: create entities and build entity map
    std::unordered_map<uint32_t, entt::entity> entityMap;
    std::unordered_map<entt::entity, uint32_t> parentMap; // Store parent IDs for second pass
    
    for (const auto& entityJson : json["entities"]) {
        if (!entityJson.contains("id")) continue;
        
        uint32_t oldId = entityJson["id"];
        std::string name = "Entity";
        
        if (entityJson.contains("components") && entityJson["components"].contains("Name")) {
            name = entityJson["components"]["Name"]["name"];
        }
        
        // Create new entity
        entt::entity newEntity = scene.CreateEntity(name);
        entityMap[oldId] = newEntity;
        
        // Store parent ID if Transform has one
        if (entityJson.contains("components") && entityJson["components"].contains("Transform")) {
            const auto& transformJson = entityJson["components"]["Transform"];
            if (transformJson.contains("parent")) {
                uint32_t parentId = transformJson["parent"];
                if (parentId != static_cast<uint32_t>(-1)) {
                    parentMap[newEntity] = parentId;
                }
            }
        }
    }
    
    // Second pass: deserialize components and resolve parent references
    for (const auto& entityJson : json["entities"]) {
        uint32_t entityId = entityJson["id"];
        entt::entity entity = entityMap[entityId];
        
        if (!entityJson.contains("components")) {
            continue;
        }
        
        const auto& components = entityJson["components"];
        
        // Deserialize Name (already created in first pass, just update)
        if (components.contains("Name")) {
            DeserializeName(registry.get<Name>(entity), components["Name"]);
        }
        
        // Deserialize Transform (resolve parent from entity map)
        if (components.contains("Transform")) {
            DeserializeTransform(registry.get<Transform>(entity), components["Transform"], registry);
            
            // Resolve parent reference
            if (parentMap.find(entity) != parentMap.end()) {
                uint32_t parentId = parentMap[entity];
                if (entityMap.find(parentId) != entityMap.end()) {
                    registry.get<Transform>(entity).parent = entityMap[parentId];
                }
            }
        }
        
        // Deserialize Renderable
        if (components.contains("Renderable")) {
            if (!registry.all_of<Renderable>(entity)) {
                registry.emplace<Renderable>(entity);
            }
            DeserializeRenderable(registry.get<Renderable>(entity), components["Renderable"]);
        }
        
        // Deserialize Camera
        if (components.contains("Camera")) {
            if (!registry.all_of<CameraComponent>(entity)) {
                registry.emplace<CameraComponent>(entity);
            }
            DeserializeCamera(registry.get<CameraComponent>(entity), components["Camera"]);
        }
        
        // Deserialize Light
        if (components.contains("Light")) {
            if (!registry.all_of<Light>(entity)) {
                registry.emplace<Light>(entity);
            }
            DeserializeLight(registry.get<Light>(entity), components["Light"]);
        }
        
        // Deserialize AABB
        if (components.contains("AABB")) {
            if (!registry.all_of<AABB>(entity)) {
                registry.emplace<AABB>(entity);
            }
            DeserializeAABB(registry.get<AABB>(entity), components["AABB"]);
        }
        
        // Deserialize Animation (simplified - just stores clip indices, not actual clips)
        if (components.contains("Animation")) {
            if (!registry.all_of<AnimationComponent>(entity)) {
                registry.emplace<AnimationComponent>(entity);
            }
            DeserializeAnimation(registry.get<AnimationComponent>(entity), components["Animation"]);
        }

        // Deserialize Animator
        if (components.contains("Animator")) {
            if (!registry.all_of<AnimatorComponent>(entity)) {
                registry.emplace<AnimatorComponent>(entity);
            }
            DeserializeAnimator(registry.get<AnimatorComponent>(entity), components["Animator"]);
        }
    }
    
    return true;
}

// ============================================================================
// Component Serialization
// ============================================================================

nlohmann::json SceneSerializer::SerializeTransform(const Transform& transform, entt::entity entity, const entt::registry& registry) {
    nlohmann::json json;
    json["position"] = {transform.position.x, transform.position.y, transform.position.z};
    json["rotation"] = {transform.rotation.w, transform.rotation.x, transform.rotation.y, transform.rotation.z};
    json["scale"] = {transform.scale.x, transform.scale.y, transform.scale.z};
    
    // Store parent entity ID (or -1 if null)
    if (transform.parent != entt::null) {
        json["parent"] = static_cast<uint32_t>(transform.parent);
    } else {
        json["parent"] = static_cast<uint32_t>(-1);
    }
    
    return json;
}

nlohmann::json SceneSerializer::SerializeName(const Name& name) {
    nlohmann::json json;
    json["name"] = name.name;
    return json;
}

nlohmann::json SceneSerializer::SerializeRenderable(const Renderable& renderable) {
    nlohmann::json json;
    json["visible"] = renderable.visible;
    json["activeLOD"] = renderable.activeLOD;
    
    // Store asset paths (meshes and materials are loaded from files, not embedded)
    // In a full implementation, you'd store GUIDs or paths
    if (renderable.mesh) {
        json["meshPath"] = ""; // TODO: Store mesh asset path/GUID
    }
    if (renderable.material) {
        json["materialPath"] = ""; // TODO: Store material asset path/GUID
    }
    
    return json;
}

nlohmann::json SceneSerializer::SerializeCamera(const CameraComponent& camera) {
    nlohmann::json json;
    json["isPrimary"] = camera.isPrimary;
    
    // Serialize camera settings
    const auto& cam = camera.camera;
    json["fov"] = cam.GetFOV();
    
    // Get position and direction vectors
    auto pos = cam.GetPosition();
    auto forward = cam.GetForward();
    auto up = cam.GetUp();
    
    json["position"] = {pos.x, pos.y, pos.z};
    json["forward"] = {forward.x, forward.y, forward.z};
    json["up"] = {up.x, up.y, up.z};
    
    // Note: near/far are stored in camera but not exposed via getters
    // We'll store defaults and they'll need to be set via SetPerspective on load
    json["nearPlane"] = 0.1f; // Default
    json["farPlane"] = 1000.0f; // Default
    json["aspect"] = 16.0f / 9.0f; // Default
    
    return json;
}

nlohmann::json SceneSerializer::SerializeLight(const Light& light) {
    nlohmann::json json;
    json["type"] = static_cast<int>(light.type);
    json["color"] = {light.color.r, light.color.g, light.color.b};
    json["intensity"] = light.intensity;
    json["range"] = light.range;
    json["innerConeAngle"] = light.innerConeAngle;
    json["outerConeAngle"] = light.outerConeAngle;
    json["castShadows"] = light.castShadows;
    return json;
}

nlohmann::json SceneSerializer::SerializeAABB(const AABB& aabb) {
    nlohmann::json json;
    json["min"] = {aabb.min.x, aabb.min.y, aabb.min.z};
    json["max"] = {aabb.max.x, aabb.max.y, aabb.max.z};
    json["isVisible"] = aabb.isVisible;
    return json;
}

nlohmann::json SceneSerializer::SerializeAnimation(const AnimationComponent& anim) {
    nlohmann::json json;
    json["currentClipIndex"] = anim.currentClipIndex;
    json["currentTime"] = anim.currentTime;
    json["isPlaying"] = anim.isPlaying;
    json["loop"] = anim.loop;
    json["playbackSpeed"] = anim.playbackSpeed;
    
    // Store skeleton path/GUID (not the skeleton itself)
    json["skeletonPath"] = ""; // TODO: Store skeleton asset path/GUID
    
    // Store clip count (actual clips loaded from files)
    json["clipCount"] = anim.clips.size();
    
    return json;
}

nlohmann::json SceneSerializer::SerializeAnimator(const AnimatorComponent& animator) {
    nlohmann::json json;
    json["controllerPath"] = animator.controllerPath;
    json["applyRootMotion"] = animator.applyRootMotion;
    json["rootMotionAffectsPhysics"] = animator.rootMotionAffectsPhysics;

    // Store basic param snapshot for convenience (does not replace controller defaults)
    // We only support float/int/bool in snapshots here (trigger is transient).
    // NOTE: Parameter definitions live in the controller asset.
    // For now, store nothing unless controller is loaded at runtime.

    return json;
}

// ============================================================================
// Component Deserialization
// ============================================================================

bool SceneSerializer::DeserializeTransform(Transform& transform, const nlohmann::json& json, const entt::registry& registry) {
    if (json.contains("position") && json["position"].is_array() && json["position"].size() == 3) {
        transform.position = glm::vec3(
            json["position"][0], json["position"][1], json["position"][2]
        );
    }
    
    if (json.contains("rotation") && json["rotation"].is_array() && json["rotation"].size() == 4) {
        transform.rotation = glm::quat(
            json["rotation"][0], json["rotation"][1], json["rotation"][2], json["rotation"][3]
        );
    }
    
    if (json.contains("scale") && json["scale"].is_array() && json["scale"].size() == 3) {
        transform.scale = glm::vec3(
            json["scale"][0], json["scale"][1], json["scale"][2]
        );
    }
    
    // Parent is resolved in DeserializeFromJson after entity map is created
    // Don't set parent here - it's handled in the calling function
    
    transform.dirty = true;
    return true;
}

bool SceneSerializer::DeserializeName(Name& name, const nlohmann::json& json) {
    if (json.contains("name") && json["name"].is_string()) {
        name.name = json["name"];
        return true;
    }
    return false;
}

bool SceneSerializer::DeserializeRenderable(Renderable& renderable, const nlohmann::json& json) {
    if (json.contains("visible")) {
        renderable.visible = json["visible"];
    }
    if (json.contains("activeLOD")) {
        renderable.activeLOD = json["activeLOD"];
    }
    
    // Mesh and material loading from paths would happen here
    // For now, we just store the reference paths
    
    return true;
}

bool SceneSerializer::DeserializeCamera(CameraComponent& camera, const nlohmann::json& json) {
    if (json.contains("isPrimary")) {
        camera.isPrimary = json["isPrimary"];
    }
    
    // Set position first
    glm::vec3 pos(0.0f);
    if (json.contains("position") && json["position"].is_array() && json["position"].size() == 3) {
        pos = glm::vec3(json["position"][0], json["position"][1], json["position"][2]);
        camera.camera.SetPosition(pos);
    }
    
    // Set target (position + forward direction)
    if (json.contains("forward") && json["forward"].is_array() && json["forward"].size() == 3) {
        glm::vec3 forward(json["forward"][0], json["forward"][1], json["forward"][2]);
        glm::vec3 target = pos + forward;
        camera.camera.SetTarget(target);
    }
    
    // Set perspective projection
    float fov = 45.0f;
    float aspect = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    
    if (json.contains("fov")) {
        fov = json["fov"];
    }
    if (json.contains("aspect")) {
        aspect = json["aspect"];
    }
    if (json.contains("nearPlane")) {
        nearPlane = json["nearPlane"];
    }
    if (json.contains("farPlane")) {
        farPlane = json["farPlane"];
    }
    
    camera.camera.SetPerspective(fov, aspect, nearPlane, farPlane);
    
    return true;
}

bool SceneSerializer::DeserializeLight(Light& light, const nlohmann::json& json) {
    if (json.contains("type")) {
        light.type = static_cast<Light::Type>(json["type"]);
    }
    if (json.contains("color") && json["color"].is_array() && json["color"].size() == 3) {
        light.color = glm::vec3(json["color"][0], json["color"][1], json["color"][2]);
    }
    if (json.contains("intensity")) {
        light.intensity = json["intensity"];
    }
    if (json.contains("range")) {
        light.range = json["range"];
    }
    if (json.contains("innerConeAngle")) {
        light.innerConeAngle = json["innerConeAngle"];
    }
    if (json.contains("outerConeAngle")) {
        light.outerConeAngle = json["outerConeAngle"];
    }
    if (json.contains("castShadows")) {
        light.castShadows = json["castShadows"];
    }
    return true;
}

bool SceneSerializer::DeserializeAABB(AABB& aabb, const nlohmann::json& json) {
    if (json.contains("min") && json["min"].is_array() && json["min"].size() == 3) {
        aabb.min = glm::vec3(json["min"][0], json["min"][1], json["min"][2]);
    }
    if (json.contains("max") && json["max"].is_array() && json["max"].size() == 3) {
        aabb.max = glm::vec3(json["max"][0], json["max"][1], json["max"][2]);
    }
    if (json.contains("isVisible")) {
        aabb.isVisible = json["isVisible"];
    }
    return true;
}

bool SceneSerializer::DeserializeAnimation(AnimationComponent& anim, const nlohmann::json& json) {
    if (json.contains("currentClipIndex")) {
        anim.currentClipIndex = json["currentClipIndex"];
    }
    if (json.contains("currentTime")) {
        anim.currentTime = json["currentTime"];
    }
    if (json.contains("isPlaying")) {
        anim.isPlaying = json["isPlaying"];
    }
    if (json.contains("loop")) {
        anim.loop = json["loop"];
    }
    if (json.contains("playbackSpeed")) {
        anim.playbackSpeed = json["playbackSpeed"];
    }
    
    // Actual skeleton and clips need to be loaded from files
    // This just restores playback state
    
    return true;
}

bool SceneSerializer::DeserializeAnimator(AnimatorComponent& animator, const nlohmann::json& json) {
    if (json.contains("controllerPath")) {
        animator.controllerPath = json["controllerPath"].get<std::string>();
    }
    if (json.contains("applyRootMotion")) {
        animator.applyRootMotion = json["applyRootMotion"].get<bool>();
    }
    if (json.contains("rootMotionAffectsPhysics")) {
        animator.rootMotionAffectsPhysics = json["rootMotionAffectsPhysics"].get<bool>();
    }

    // Controller is loaded lazily by AnimatorSystem (runtime).
    animator.controller.reset();
    animator.layers.clear();
    animator.recentEvents.clear();
    return true;
}

// Note: CreateEntityMap is no longer needed - entity creation is handled in DeserializeFromJson

} // namespace ECS
} // namespace OpenEye

