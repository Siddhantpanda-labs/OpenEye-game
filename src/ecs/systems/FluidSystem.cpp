#include "FluidSystem.h"
#include "../Scene.h"
#include "../Components.h"
#include "../../fluids/FluidSimulation.h"
#include "../../fluids/FluidRenderer.h"
#include "../../rendering/Camera.h"
#include "../../core/log/Log.h"
#include <vector>
#include <algorithm>

using namespace OpenEye::Core;

namespace OpenEye {
namespace ECS {

bool FluidSystem::s_initialized = false;
std::unique_ptr<OpenEye::Fluids::FluidRenderer> FluidSystem::s_renderer = nullptr;

void FluidSystem::Update(Scene& scene, float deltaTime) {
	auto& registry = scene.GetRegistry();
	// Iterate volumes if any
	if (!registry.storage<FluidVolumeComponent>()) return;

    // Debug: read back density once per second to confirm the sim is actually writing anything.
    // This is the fastest way to split "simulation is empty" vs "renderer is broken".
    static float s_densityReadbackTimer = 0.0f;
    s_densityReadbackTimer += deltaTime;
    const bool doReadback = (s_densityReadbackTimer >= 1.0f);
    if (doReadback) s_densityReadbackTimer = 0.0f;

	for (auto entity : registry.view<FluidVolumeComponent>()) {
		auto& vol = registry.get<FluidVolumeComponent>(entity);
		if (!vol.simulation || !vol.enabled) continue;
		vol.simulation->Step(deltaTime);

        if (doReadback) {
            const glm::ivec3 res = vol.simulation->GetResolution();
            const std::size_t count = (std::size_t)res.x * (std::size_t)res.y * (std::size_t)res.z;
            if (count > 0) {
                std::vector<float> density(count, 0.0f);
                const GLuint tex = vol.simulation->GetDensityTexture();
                glBindTexture(GL_TEXTURE_3D, tex);
                glGetTexImage(GL_TEXTURE_3D, 0, GL_RED, GL_FLOAT, density.data());
                glBindTexture(GL_TEXTURE_3D, 0);

                float maxD = 0.0f;
                for (float v : density) maxD = std::max(maxD, v);
                const int cx = res.x / 2, cy = res.y / 2, cz = res.z / 2;
                const std::size_t centerIdx = (std::size_t)cx + (std::size_t)cy * (std::size_t)res.x + (std::size_t)cz * (std::size_t)res.x * (std::size_t)res.y;
                const float centerD = (centerIdx < density.size()) ? density[centerIdx] : 0.0f;

                OE_INFO_F("FluidDebug: density max=%.6f center=%.6f (res=%dx%dx%d)", maxD, centerD, res.x, res.y, res.z);
            }
        }
	}
}

void FluidSystem::Render(Scene& scene, const Rendering::Camera& camera,
						  const glm::mat4& view, const glm::mat4& proj) {
	if (!s_initialized) {
		s_renderer = std::make_unique<OpenEye::Fluids::FluidRenderer>();
		if (!s_renderer->Initialize()) {
			OE_ERROR("FluidSystem: Failed to initialize renderer");
			return;
		}
		s_initialized = true;
	}

	auto& registry = scene.GetRegistry();
	for (auto entity : registry.view<Transform, FluidVolumeComponent>()) {
		auto& transform = registry.get<Transform>(entity);
		auto& vol = registry.get<FluidVolumeComponent>(entity);
		if (!vol.simulation || !vol.enabled) continue;

		glm::mat4 model = transform.GetWorldMatrix(registry);
		s_renderer->Render(*vol.simulation, camera, view, proj, model);
	}
}

} // namespace ECS
} // namespace OpenEye


