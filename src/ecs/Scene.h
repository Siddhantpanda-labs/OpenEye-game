#pragma once

#include "entt/entt.hpp"

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
    
    Registry& GetRegistry() { return m_registry; }
    const Registry& GetRegistry() const { return m_registry; }

private:
    Registry m_registry;
};

} // namespace ECS
} // namespace OpenEye

