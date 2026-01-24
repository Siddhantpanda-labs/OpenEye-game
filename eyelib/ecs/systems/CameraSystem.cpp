#include "CameraSystem.h"

#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/Camera.h"
#include "eyelib/ecs/components/Transform.h"

#include <glm/gtc/matrix_transform.hpp>

namespace eyelib {
namespace ecs {

void CameraSystem::Update(World& world, float /*deltaTime*/) {
    auto view = world.Registry().view<components::Transform, components::Camera>();
    for (auto entity : view) {
        auto& transform = view.get<components::Transform>(entity);
        auto& camera = view.get<components::Camera>(entity);

        math::Mat4 worldMat = transform.GetWorldMatrix(world);
        camera.camera.view = glm::inverse(worldMat);
        camera.camera.viewPos = math::Vec3(worldMat[3]);

        if (camera.isPerspective) {
            camera.camera.projection = glm::perspective(glm::radians(camera.fov),
                camera.aspect, camera.nearPlane, camera.farPlane);
        } else {
            const float half = camera.orthoSize * 0.5f;
            camera.camera.projection = glm::ortho(-half * camera.aspect, half * camera.aspect,
                -half, half, camera.nearPlane, camera.farPlane);
        }
        camera.camera.nearPlane = camera.nearPlane;
        camera.camera.farPlane = camera.farPlane;
    }
}

} // namespace ecs
} // namespace eyelib
