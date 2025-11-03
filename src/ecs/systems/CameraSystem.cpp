#include "CameraSystem.h"
#include "../Scene.h"
#include "../Components.h"

namespace OpenEye {
namespace ECS {

entt::entity CameraSystem::FindPrimaryCamera(entt::registry& registry) {
    auto view = registry.view<CameraComponent>();
    
    for (auto entity : view) {
        const auto& cameraComp = view.get<CameraComponent>(entity);
        if (cameraComp.isPrimary) {
            return entity;
        }
    }
    
    // Return first camera if no primary found
    if (!view.empty()) {
        return *view.begin();
    }
    
    return entt::null;
}

Rendering::Camera* CameraSystem::GetPrimaryCamera(entt::registry& registry) {
    entt::entity cameraEntity = FindPrimaryCamera(registry);
    if (cameraEntity != entt::null && registry.valid(cameraEntity)) {
        if (registry.all_of<CameraComponent>(cameraEntity)) {
            return &registry.get<CameraComponent>(cameraEntity).camera;
        }
    }
    return nullptr;
}

} // namespace ECS
} // namespace OpenEye

