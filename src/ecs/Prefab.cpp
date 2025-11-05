#include "Prefab.h"
#include "SceneSerializer.h"
#include "core/log/Log.h"
#include <fstream>
#include <functional>
#include <glm/gtc/quaternion.hpp>

namespace OpenEye {
namespace ECS {

// Static prefab registry
std::unordered_map<std::string, std::shared_ptr<Prefab>> PrefabManager::s_prefabs;

// ============================================================================
// Prefab Implementation
// ============================================================================

Prefab::Prefab() : m_name("UnnamedPrefab") {
    m_data["version"] = 1;
    m_data["entities"] = nlohmann::json::array();
}

std::shared_ptr<Prefab> Prefab::CreateFromEntity(Scene& scene, Entity entity, const std::string& name) {
    auto prefab = std::make_shared<Prefab>();
    prefab->SetName(name);
    
    auto& registry = scene.GetRegistry();
    if (!registry.valid(entity)) {
        OE_ERROR("Prefab::CreateFromEntity: Invalid entity");
        return nullptr;
    }
    
    // Build entity map (just root entity for now)
    std::unordered_map<Entity, uint32_t> entityMap;
    entityMap[entity] = 0;
    
    // Serialize entity hierarchy (single entity)
    prefab->m_data = SerializeEntityHierarchy(scene, entity, entityMap);
    
    return prefab;
}

std::shared_ptr<Prefab> Prefab::CreateFromHierarchy(Scene& scene, Entity rootEntity, const std::string& name) {
    auto prefab = std::make_shared<Prefab>();
    prefab->SetName(name);
    
    auto& registry = scene.GetRegistry();
    if (!registry.valid(rootEntity)) {
        OE_ERROR("Prefab::CreateFromHierarchy: Invalid root entity");
        return nullptr;
    }
    
    // Build entity map (collect all entities in hierarchy)
    std::unordered_map<Entity, uint32_t> entityMap;
    
    // Collect all entities (we'll do a full scene scan for now)
    // In a full implementation, you'd traverse the hierarchy properly
    uint32_t nextId = 0;
    auto view = registry.view<Transform>();
    for (auto e : view) {
        // Include root entity and all entities that have root as parent
        bool isInHierarchy = (e == rootEntity);
        if (!isInHierarchy && registry.all_of<Transform>(e)) {
            const auto& transform = registry.get<Transform>(e);
            Entity current = e;
            // Traverse up parent chain to see if root is ancestor
            while (transform.parent != entt::null && current != rootEntity) {
                current = transform.parent;
                if (current == rootEntity) {
                    isInHierarchy = true;
                    break;
                }
                if (!registry.all_of<Transform>(current)) break;
                const auto& parentTransform = registry.get<Transform>(current);
                if (parentTransform.parent == entt::null) break;
                current = parentTransform.parent;
            }
        }
        if (isInHierarchy) {
            entityMap[e] = nextId++;
        }
    }
    
    // Serialize entity hierarchy
    prefab->m_data = SerializeEntityHierarchy(scene, rootEntity, entityMap);
    
    return prefab;
}

Entity Prefab::Instantiate(Scene& scene, const glm::vec3* position, const glm::quat* rotation) const {
    if (!m_data.contains("entities") || !m_data["entities"].is_array()) {
        OE_ERROR("Prefab::Instantiate: Invalid prefab data");
        return entt::null;
    }
    
    return DeserializeEntityHierarchy(scene, m_data, position, rotation);
}

bool Prefab::SaveToFile(const std::string& filepath) const {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            OE_ERROR_F("Prefab::SaveToFile: Failed to open file for writing: %s", filepath.c_str());
            return false;
        }
        
        nlohmann::json json = m_data;
        json["prefabName"] = m_name;
        
        file << json.dump(2); // Pretty print
        file.close();
        
        OE_INFO_F("Prefab::SaveToFile: Successfully saved prefab '%s' to %s", m_name.c_str(), filepath.c_str());
        return true;
    } catch (const std::exception& e) {
        OE_ERROR_F("Prefab::SaveToFile: Exception: %s", e.what());
        return false;
    }
}

std::shared_ptr<Prefab> Prefab::LoadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            OE_ERROR_F("Prefab::LoadFromFile: Failed to open file: %s", filepath.c_str());
            return nullptr;
        }
        
        nlohmann::json json;
        file >> json;
        file.close();
        
        auto prefab = std::make_shared<Prefab>();
        
        if (json.contains("prefabName")) {
            prefab->SetName(json["prefabName"]);
        }
        
        // Copy entity data (remove prefabName from data)
        prefab->m_data = json;
        prefab->m_data.erase("prefabName");
        
        OE_INFO_F("Prefab::LoadFromFile: Successfully loaded prefab '%s' from %s", prefab->GetName().c_str(), filepath.c_str());
        return prefab;
    } catch (const std::exception& e) {
        OE_ERROR_F("Prefab::LoadFromFile: Exception: %s", e.what());
        return nullptr;
    }
}

nlohmann::json Prefab::SerializeEntityHierarchy(Scene& scene, Entity rootEntity, std::unordered_map<Entity, uint32_t>& entityMap) {
    nlohmann::json json;
    json["version"] = 1;
    json["entities"] = nlohmann::json::array();
    
    auto& registry = scene.GetRegistry();
    
    // Serialize all entities in the map
    for (const auto& [entity, id] : entityMap) {
        if (!registry.valid(entity)) continue;
        
        nlohmann::json entityJson;
        entityJson["id"] = id;
        entityJson["isRoot"] = (entity == rootEntity);
        entityJson["components"] = nlohmann::json::object();
        
        // Serialize Name
        if (registry.all_of<Name>(entity)) {
            entityJson["components"]["Name"] = SceneSerializer::SerializeName(registry.get<Name>(entity));
        }
        
        // Serialize Transform
        if (registry.all_of<Transform>(entity)) {
            entityJson["components"]["Transform"] = SceneSerializer::SerializeTransform(
                registry.get<Transform>(entity), entity, registry);
        }
        
        // Serialize Renderable
        if (registry.all_of<Renderable>(entity)) {
            entityJson["components"]["Renderable"] = SceneSerializer::SerializeRenderable(registry.get<Renderable>(entity));
        }
        
        // Serialize Camera
        if (registry.all_of<CameraComponent>(entity)) {
            entityJson["components"]["Camera"] = SceneSerializer::SerializeCamera(registry.get<CameraComponent>(entity));
        }
        
        // Serialize Light
        if (registry.all_of<Light>(entity)) {
            entityJson["components"]["Light"] = SceneSerializer::SerializeLight(registry.get<Light>(entity));
        }
        
        // Serialize AABB
        if (registry.all_of<AABB>(entity)) {
            entityJson["components"]["AABB"] = SceneSerializer::SerializeAABB(registry.get<AABB>(entity));
        }
        
        // Serialize Animation
        if (registry.all_of<AnimationComponent>(entity)) {
            entityJson["components"]["Animation"] = SceneSerializer::SerializeAnimation(registry.get<AnimationComponent>(entity));
        }
        
        json["entities"].push_back(entityJson);
    }
    
    return json;
}

Entity Prefab::DeserializeEntityHierarchy(Scene& scene, const nlohmann::json& json, const glm::vec3* position, const glm::quat* rotation) {
    if (!json.contains("entities") || !json["entities"].is_array()) {
        OE_ERROR("Prefab::DeserializeEntityHierarchy: Invalid JSON format");
        return entt::null;
    }
    
    auto& registry = scene.GetRegistry();
    
    // First pass: create entities and build entity map
    std::unordered_map<uint32_t, Entity> entityMap;
    std::unordered_map<Entity, uint32_t> parentMap;
    Entity rootEntity = entt::null;
    
    for (const auto& entityJson : json["entities"]) {
        if (!entityJson.contains("id")) continue;
        
        uint32_t oldId = entityJson["id"];
        std::string name = "Entity";
        bool isRoot = false;
        
        if (entityJson.contains("isRoot")) {
            isRoot = entityJson["isRoot"];
        }
        
        if (entityJson.contains("components") && entityJson["components"].contains("Name")) {
            name = entityJson["components"]["Name"]["name"];
        }
        
        // Create new entity
        Entity newEntity = scene.CreateEntity(name);
        entityMap[oldId] = newEntity;
        
        if (isRoot) {
            rootEntity = newEntity;
        }
        
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
        Entity entity = entityMap[entityId];
        
        if (!entityJson.contains("components")) {
            continue;
        }
        
        const auto& components = entityJson["components"];
        
        // Deserialize Name
        if (components.contains("Name")) {
            SceneSerializer::DeserializeName(registry.get<Name>(entity), components["Name"]);
        }
        
        // Deserialize Transform (with optional position/rotation override for root)
        if (components.contains("Transform")) {
            SceneSerializer::DeserializeTransform(registry.get<Transform>(entity), components["Transform"], registry);
            
            // Apply position/rotation override if this is the root entity
            if (entity == rootEntity) {
                if (position) {
                    registry.get<Transform>(entity).position = *position;
                }
                if (rotation) {
                    registry.get<Transform>(entity).rotation = *rotation;
                }
            }
            
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
            SceneSerializer::DeserializeRenderable(registry.get<Renderable>(entity), components["Renderable"]);
        }
        
        // Deserialize Camera
        if (components.contains("Camera")) {
            if (!registry.all_of<CameraComponent>(entity)) {
                registry.emplace<CameraComponent>(entity);
            }
            SceneSerializer::DeserializeCamera(registry.get<CameraComponent>(entity), components["Camera"]);
        }
        
        // Deserialize Light
        if (components.contains("Light")) {
            if (!registry.all_of<Light>(entity)) {
                registry.emplace<Light>(entity);
            }
            SceneSerializer::DeserializeLight(registry.get<Light>(entity), components["Light"]);
        }
        
        // Deserialize AABB
        if (components.contains("AABB")) {
            if (!registry.all_of<AABB>(entity)) {
                registry.emplace<AABB>(entity);
            }
            SceneSerializer::DeserializeAABB(registry.get<AABB>(entity), components["AABB"]);
        }
        
        // Deserialize Animation
        if (components.contains("Animation")) {
            if (!registry.all_of<AnimationComponent>(entity)) {
                registry.emplace<AnimationComponent>(entity);
            }
            SceneSerializer::DeserializeAnimation(registry.get<AnimationComponent>(entity), components["Animation"]);
        }
    }
    
    return rootEntity;
}

// ============================================================================
// PrefabManager Implementation
// ============================================================================

bool PrefabManager::LoadPrefab(const std::string& name, const std::string& filepath) {
    auto prefab = Prefab::LoadFromFile(filepath);
    if (!prefab) {
        return false;
    }
    
    RegisterPrefab(name, prefab);
    return true;
}

void PrefabManager::RegisterPrefab(const std::string& name, std::shared_ptr<Prefab> prefab) {
    if (s_prefabs.find(name) != s_prefabs.end()) {
        OE_WARN_F("PrefabManager: Overwriting existing prefab '%s'", name.c_str());
    }
    s_prefabs[name] = prefab;
    OE_INFO_F("PrefabManager: Registered prefab '%s'", name.c_str());
}

std::shared_ptr<Prefab> PrefabManager::GetPrefab(const std::string& name) {
    auto it = s_prefabs.find(name);
    if (it != s_prefabs.end()) {
        return it->second;
    }
    return nullptr;
}

Entity PrefabManager::Instantiate(Scene& scene, const std::string& name, const glm::vec3* position, const glm::quat* rotation) {
    auto prefab = GetPrefab(name);
    if (!prefab) {
        OE_WARN_F("PrefabManager::Instantiate: Prefab '%s' not found", name.c_str());
        return entt::null;
    }
    
    return prefab->Instantiate(scene, position, rotation);
}

bool PrefabManager::HasPrefab(const std::string& name) {
    return s_prefabs.find(name) != s_prefabs.end();
}

void PrefabManager::Clear() {
    s_prefabs.clear();
    OE_INFO("PrefabManager: Cleared all prefabs");
}

std::vector<std::string> PrefabManager::GetPrefabNames() {
    std::vector<std::string> names;
    names.reserve(s_prefabs.size());
    for (const auto& [name, prefab] : s_prefabs) {
        names.push_back(name);
    }
    return names;
}

} // namespace ECS
} // namespace OpenEye

