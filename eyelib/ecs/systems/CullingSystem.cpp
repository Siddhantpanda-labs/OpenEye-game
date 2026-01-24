#include "CullingSystem.h"

#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/AABB.h"
#include "eyelib/ecs/components/Camera.h"
#include "eyelib/ecs/components/Transform.h"
#include "eyelib/math/Frustum.h"

namespace eyelib {
namespace ecs {

void CullingSystem::Update(World& world, float /*deltaTime*/) {
    math::Frustum frustum;
    bool hasCamera = false;

    auto camView = world.Registry().view<components::Camera>();
    for (auto entity : camView) {
        const auto& cam = camView.get<components::Camera>(entity);
        if (!cam.isPrimary) continue;
        frustum.FromMatrix(cam.camera.projection * cam.camera.view);
        hasCamera = true;
        break;
    }

    if (!hasCamera) {
        auto it = camView.begin();
        if (it != camView.end()) {
            const auto& cam = camView.get<components::Camera>(*it);
            frustum.FromMatrix(cam.camera.projection * cam.camera.view);
            hasCamera = true;
        }
    }

    if (!hasCamera) return;

    auto view = world.Registry().view<components::Transform, components::AABB>();
    for (auto entity : view) {
        auto& transform = view.get<components::Transform>(entity);
        auto& bounds = view.get<components::AABB>(entity);

        const auto worldBounds = bounds.Transform(transform.GetWorldMatrix(world));
        bounds.isVisible = frustum.Contains(eyelib::math::AABB{worldBounds.min, worldBounds.max}) != 0;
    }
}

} // namespace ecs
} // namespace eyelib
