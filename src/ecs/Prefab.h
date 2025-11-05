#pragma once

#include "Scene.h"
#include "Components.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <json.hpp>

namespace OpenEye {
namespace ECS {

/**
 * @brief Prefab represents a template entity that can be instantiated multiple times.
 * 
 * Prefabs are stored as JSON and can be instantiated into scenes.
 * They support:
 * - Entity hierarchies (parent-child relationships)
 * - All component types
 * - Overrideable properties
 * - Nested prefabs
 */
class Prefab {
public:
    Prefab();
    ~Prefab() = default;
    
    /**
     * @brief Create a prefab from a scene entity.
     * @param scene Scene containing the entity
     * @param entity Entity to convert to prefab
     * @param name Name of the prefab
     * @return Created prefab, or nullptr on failure
     */
    static std::shared_ptr<Prefab> CreateFromEntity(Scene& scene, Entity entity, const std::string& name);
    
    /**
     * @brief Create a prefab from multiple entities (hierarchy).
     * @param scene Scene containing the entities
     * @param rootEntity Root entity of the hierarchy
     * @param name Name of the prefab
     * @return Created prefab, or nullptr on failure
     */
    static std::shared_ptr<Prefab> CreateFromHierarchy(Scene& scene, Entity rootEntity, const std::string& name);
    
    /**
     * @brief Instantiate this prefab into a scene.
     * @param scene Target scene
     * @param position Optional override position (if provided, overrides root entity position)
     * @param rotation Optional override rotation (if provided, overrides root entity rotation)
     * @return Root entity of the instantiated hierarchy, or null entity on failure
     */
    Entity Instantiate(Scene& scene, const glm::vec3* position = nullptr, const glm::quat* rotation = nullptr) const;
    
    /**
     * @brief Save prefab to JSON file.
     * @param filepath Path to save file
     * @return true if successful, false otherwise
     */
    bool SaveToFile(const std::string& filepath) const;
    
    /**
     * @brief Load prefab from JSON file.
     * @param filepath Path to load file from
     * @return Created prefab, or nullptr on failure
     */
    static std::shared_ptr<Prefab> LoadFromFile(const std::string& filepath);
    
    /**
     * @brief Get prefab name.
     */
    const std::string& GetName() const { return m_name; }
    
    /**
     * @brief Set prefab name.
     */
    void SetName(const std::string& name) { m_name = name; }
    
    /**
     * @brief Get prefab JSON data.
     */
    const nlohmann::json& GetData() const { return m_data; }
    
    /**
     * @brief Set prefab JSON data.
     */
    void SetData(const nlohmann::json& data) { m_data = data; }

private:
    std::string m_name;
    nlohmann::json m_data; // JSON representation of the prefab entity hierarchy
    
    // Helper: Serialize entity hierarchy to JSON
    static nlohmann::json SerializeEntityHierarchy(Scene& scene, Entity rootEntity, std::unordered_map<Entity, uint32_t>& entityMap);
    
    // Helper: Deserialize and create entity hierarchy from JSON
    static Entity DeserializeEntityHierarchy(Scene& scene, const nlohmann::json& json, const glm::vec3* position = nullptr, const glm::quat* rotation = nullptr);
};

/**
 * @brief PrefabManager manages prefab assets and provides instantiation.
 */
class PrefabManager {
public:
    /**
     * @brief Load a prefab from file and register it.
     * @param name Prefab name (identifier)
     * @param filepath Path to prefab file
     * @return true if successful, false otherwise
     */
    static bool LoadPrefab(const std::string& name, const std::string& filepath);
    
    /**
     * @brief Register a prefab.
     * @param name Prefab name (identifier)
     * @param prefab Prefab instance
     */
    static void RegisterPrefab(const std::string& name, std::shared_ptr<Prefab> prefab);
    
    /**
     * @brief Get a prefab by name.
     * @param name Prefab name
     * @return Prefab instance, or nullptr if not found
     */
    static std::shared_ptr<Prefab> GetPrefab(const std::string& name);
    
    /**
     * @brief Instantiate a prefab by name into a scene.
     * @param scene Target scene
     * @param name Prefab name
     * @param position Optional override position
     * @param rotation Optional override rotation
     * @return Root entity of the instantiated hierarchy, or null entity on failure
     */
    static Entity Instantiate(Scene& scene, const std::string& name, const glm::vec3* position = nullptr, const glm::quat* rotation = nullptr);
    
    /**
     * @brief Check if a prefab exists.
     * @param name Prefab name
     * @return true if prefab is registered, false otherwise
     */
    static bool HasPrefab(const std::string& name);
    
    /**
     * @brief Clear all registered prefabs.
     */
    static void Clear();
    
    /**
     * @brief Get all registered prefab names.
     * @return Vector of prefab names
     */
    static std::vector<std::string> GetPrefabNames();

private:
    static std::unordered_map<std::string, std::shared_ptr<Prefab>> s_prefabs;
};

} // namespace ECS
} // namespace OpenEye

