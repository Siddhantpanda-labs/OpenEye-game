#pragma once

#include "../Scene.h"
#include "../../fluids/FluidCommon.h"

namespace OpenEye {
namespace Fluids { class FluidSimulation; class FluidRenderer; }
namespace Rendering { class Camera; }
namespace ECS {

class FluidSystem {
public:
	static void Update(Scene& scene, float deltaTime);
	static void Render(Scene& scene, const Rendering::Camera& camera,
					  const glm::mat4& view, const glm::mat4& proj);

private:
	static bool s_initialized;
	static std::unique_ptr<Fluids::FluidRenderer> s_renderer;
};

} // namespace ECS
} // namespace OpenEye


