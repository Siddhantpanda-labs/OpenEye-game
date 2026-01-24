#include "RenderSystem.h"

#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/AABB.h"
#include "eyelib/ecs/components/Camera.h"
#include "eyelib/ecs/components/Light.h"
#include "eyelib/ecs/components/Renderable.h"
#include "eyelib/ecs/components/Transform.h"
#include "eyelib/render/RenderPipeline.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eyelib {
namespace ecs {

namespace {
math::Vec3 ForwardFromRotation(const math::Quat& q) {
    return glm::normalize(q * math::Vec3(0.0f, 0.0f, -1.0f));
}
} // namespace

void RenderSystem::Update(World& world, float /*deltaTime*/) {
    auto* pipeline = world.GetRenderPipeline();
    if (!pipeline) return;

    render::RenderScene scene;

    // Camera
    bool hasCamera = false;
    auto camView = world.Registry().view<components::Camera>();
    for (auto entity : camView) {
        const auto& cam = camView.get<components::Camera>(entity);
        if (!cam.isPrimary) continue;
        scene.camera = cam.camera;
        hasCamera = true;
        break;
    }
    if (!hasCamera) {
        auto it = camView.begin();
        if (it != camView.end()) {
            const auto& cam = camView.get<components::Camera>(*it);
            scene.camera = cam.camera;
            hasCamera = true;
        }
    }

    // Lights
    auto lightView = world.Registry().view<components::Transform, components::Light>();
    for (auto entity : lightView) {
        const auto& transform = lightView.get<components::Transform>(entity);
        const auto& light = lightView.get<components::Light>(entity);
        if (light.type == components::Light::Type::Directional) {
            scene.light.direction = ForwardFromRotation(transform.rotation);
            scene.light.color = light.color;
            scene.light.intensity = light.intensity;
        } else if (light.type == components::Light::Type::Point) {
            render::PointLight pl;
            pl.position = transform.position;
            pl.color = light.color;
            pl.intensity = light.intensity;
            pl.radius = light.range;
            scene.pointLights.push_back(pl);
        } else if (light.type == components::Light::Type::Spot) {
            render::SpotLight sl;
            sl.position = transform.position;
            sl.direction = ForwardFromRotation(transform.rotation);
            sl.color = light.color;
            sl.intensity = light.intensity;
            sl.range = light.range;
            sl.innerConeCos = std::cos(glm::radians(light.innerConeAngle));
            sl.outerConeCos = std::cos(glm::radians(light.outerConeAngle));
            scene.spotLights.push_back(sl);
        }
    }

    // Renderables
    auto view = world.Registry().view<components::Transform, components::Renderable>();
    for (auto entity : view) {
        const auto& transform = view.get<components::Transform>(entity);
        const auto& renderable = view.get<components::Renderable>(entity);

        if (!renderable.visible) continue;
        if (auto* bounds = world.Registry().try_get<components::AABB>(entity)) {
            if (!bounds->isVisible) continue;
        }

        auto mesh = renderable.mesh;
        if (!renderable.lodMeshes.empty() && renderable.activeLOD < renderable.lodMeshes.size()) {
            mesh = renderable.lodMeshes[renderable.activeLOD];
        }

        if (!mesh || !renderable.material) continue;

        render::Drawable d;
        d.mesh = mesh;
        d.material = renderable.material;
        d.transform = transform.GetWorldMatrix(world);
        scene.opaque.push_back(d);
    }

    if (hasCamera) {
        pipeline->Render(scene);
    }
}

} // namespace ecs
} // namespace eyelib
