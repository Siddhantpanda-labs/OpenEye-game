#pragma once

#include "entt/entt.hpp"
#include <glm/glm.hpp>

// Forward declaration
namespace OpenEye {
namespace Physics {
    class PhysicsWorld;
}
}

namespace OpenEye {
namespace ECS {

using Entity = entt::entity;
using Registry = entt::registry;

class Scene {
public:
    Scene();
    ~Scene();
    
    Entity CreateEntity(const std::string& name = "Entity");
    void DestroyEntity(Entity entity);
    
    /**
     * @brief Load a 3D model from file and add it to the scene.
     * @param filepath Path to model file (glTF, FBX, etc.)
     * @param name Optional name for the root entity (defaults to filename)
     * @param position Optional position for the model (defaults to origin)
     * @return Root entity of the loaded model, or null entity on failure
     */
    Entity LoadModel(const std::string& filepath, const std::string& name = "", const glm::vec3& position = glm::vec3(0.0f));
    
    /**
     * @brief Update all ECS systems in the scene.
     * @param deltaTime Time since last frame (in seconds)
     * @param physicsWorld Optional physics world pointer (void* from API::PhysicsWorld::GetInternalWorld()). If provided, physics simulation will be updated.
     */
    void Update(float deltaTime, void* physicsWorld = nullptr);
    
    Registry& GetRegistry() { return m_registry; }
    const Registry& GetRegistry() const { return m_registry; }

private:
    Registry m_registry;
};

} // namespace ECS
} // namespace OpenEye

