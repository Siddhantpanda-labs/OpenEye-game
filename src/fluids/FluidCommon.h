#pragma once

#include <memory>
#include <glm/glm.hpp>

namespace OpenEye {
namespace Fluids {

enum class FluidType {
	Gas,
	Liquid
};

struct FluidGridDesc {
	glm::ivec3 resolution{64, 64, 64};
	glm::vec3  volumeSize{1.0f, 1.0f, 1.0f};
};

} // namespace Fluids
} // namespace OpenEye


