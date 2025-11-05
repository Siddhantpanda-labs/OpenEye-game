#include "FluidSystem.h"
#include "../Scene.h"
#include "../Components.h"
#include "../../fluids/FluidSimulation.h"
#include "../../fluids/FluidRenderer.h"
#include "../../rendering/Camera.h"
#include "../../core/log/Log.h"

using namespace OpenEye::Core;

namespace OpenEye {
namespace ECS {

bool FluidSystem::s_initialized = false;
std::unique_ptr<OpenEye::Fluids::FluidRenderer> FluidSystem::s_renderer = nullptr;

void FluidSystem::Update(Scene& scene, float deltaTime) {
	auto& registry = scene.GetRegistry();
	// Iterate volumes if any
	if (!registry.storage<FluidVolumeComponent>()) return;

	for (auto entity : registry.view<FluidVolumeComponent>()) {
		auto& vol = registry.get<FluidVolumeComponent>(entity);
		if (!vol.simulation || !vol.enabled) continue;
		vol.simulation->Step(deltaTime);
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


