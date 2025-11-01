#include "Scene.h"
#include "Components.h"
#include "core/log/Log.h"

namespace OpenEye {
namespace ECS {

Scene::Scene() {
    OE_DEBUG("Scene created");
}

Scene::~Scene() {
    OE_DEBUG("Scene destroyed");
}

Entity Scene::CreateEntity(const std::string& name) {
    Entity entity = m_registry.create();
    
    // Add Name component
    m_registry.emplace<Name>(entity, name);
    
    // Add Transform component (default)
    m_registry.emplace<Transform>(entity);
    
    return entity;
}

void Scene::DestroyEntity(Entity entity) {
    if (!m_registry.valid(entity)) {
        return;
    }
    
    // Remove from parent's children before destroying
    if (m_registry.all_of<Transform>(entity)) {
        auto& transform = m_registry.get<Transform>(entity);
        transform.parent = entt::null;
    }
    
    m_registry.destroy(entity);
}

} // namespace ECS
} // namespace OpenEye

