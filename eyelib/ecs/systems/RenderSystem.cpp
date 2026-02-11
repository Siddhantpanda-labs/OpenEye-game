#include "RenderSystem.h"

#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/AABB.h"
#include "eyelib/ecs/components/Camera.h"
#include "eyelib/ecs/components/FluidVolume.h"
#include "eyelib/ecs/components/Light.h"
#include "eyelib/ecs/components/Renderable.h"
#include "eyelib/ecs/components/Transform.h"
#include "eyelib/fluids/FluidSurfaceExtractor.h"
#include "eyelib/fluids/FluidVolumeRenderer.h"
#include "eyelib/render/RenderPipeline.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Shader.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace eyelib {
namespace ecs {

namespace {
math::Vec3 ForwardFromRotation(const math::Quat& q) {
    return glm::normalize(q * math::Vec3(0.0f, 0.0f, -1.0f));
}

struct FluidRenderItem {
    std::shared_ptr<fluids::FluidSimulation> simulation;
    math::Mat4 model{1.0f};
    math::Vec3 volumeSize{1.0f};
    float densityScale = 1.0f;
};
} // namespace

void RenderSystem::Update(World& world, float deltaTime) {
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

    // Fluids (volume + surface + foam)
    static fluids::FluidVolumeRenderer s_volumeRenderer;
    static bool s_volumeInit = false;
    static fluids::FluidSurfaceExtractor s_surfaceExtractor;
    static std::shared_ptr<render::Material> s_surfaceMaterial;

    std::vector<FluidRenderItem> volumesToRender;
    auto fluidView = world.Registry().view<components::Transform, components::FluidVolume>();
    for (auto entity : fluidView) {
        auto& transform = fluidView.get<components::Transform>(entity);
        auto& volume = fluidView.get<components::FluidVolume>(entity);
        if (!volume.simulation || !volume.enabled) continue;

        // Update simulation
        volume.simulation->Step(deltaTime);

        const math::Mat4 model = transform.GetWorldMatrix(world);

        if (volume.renderVolume) {
            volumesToRender.push_back({ volume.simulation, model, volume.volumeSize, volume.densityScale });
        }

        if (volume.renderSurface) {
            if (!s_surfaceMaterial) {
                auto shader = std::make_shared<render::Shader>();
                shader->LoadFromFile("passes/forward/basic", "passes/forward/basic");
                s_surfaceMaterial = std::make_shared<render::Material>(shader);
                s_surfaceMaterial->SetAlbedo(math::Vec3(0.25f, 0.45f, 0.9f));
                s_surfaceMaterial->SetAlpha(0.75f);
                s_surfaceMaterial->SetRoughness(0.15f);
                s_surfaceMaterial->SetMetallic(0.0f);
            }
            auto mesh = s_surfaceExtractor.BuildMesh(*volume.simulation, volume.surfaceIso);
            if (mesh && mesh->GetIndexCount() > 0) {
                render::Drawable d;
                d.mesh = mesh;
                d.material = volume.surfaceMaterial ? volume.surfaceMaterial : s_surfaceMaterial;
                d.transform = model;
                scene.transparent.push_back(d);
            }
        }

        if (volume.renderFoam) {
            if (!volume.foamRenderer) {
                volume.foamRenderer = std::make_shared<fluids::FluidFoamRenderer>();
            }
            volume.foamRenderer->SetSettings(volume.foamSettings);
            volume.foamRenderer->UpdateFromSimulation(*volume.simulation, deltaTime);
            auto mesh = volume.foamRenderer->BuildMesh(scene.camera);
            if (mesh && mesh->GetIndexCount() > 0) {
                render::Drawable d;
                d.mesh = mesh;
                d.material = volume.foamRenderer->GetMaterial();
                d.transform = model;
                scene.transparent.push_back(d);
            }
        }
    }

    if (hasCamera) {
        if (!volumesToRender.empty()) {
            if (!s_volumeInit) {
                s_volumeInit = s_volumeRenderer.Initialize();
            }
            auto* renderer = &s_volumeRenderer;
            pipeline->SetPrePostProcessCallback([volumes = std::move(volumesToRender), renderer](render::rhi::IRenderTarget* hdrRT, unsigned int width, unsigned int height, const render::RenderScene& scene) {
                (void)width;
                (void)height;
                if (!hdrRT) return;
                unsigned int hdrFbo = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(hdrRT->GetNativeFramebuffer()));
                if (hdrFbo == 0) return;
                for (const auto& item : volumes) {
                    if (!item.simulation) continue;
                    renderer->UpdateFromSimulation(*item.simulation, item.densityScale);
                    renderer->RenderVolume(*item.simulation, scene.camera, item.model,
                                           math::Vec3(0.5f), item.volumeSize, hdrFbo);
                }
            });
        } else {
            pipeline->SetPrePostProcessCallback(nullptr);
        }
        pipeline->Render(scene);
    }
}

} // namespace ecs
} // namespace eyelib
