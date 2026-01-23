#pragma once

// Use GLM as the math backend (engine-internal).
// We keep a stable "eyelib::math" surface so we can refactor later if needed.

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

namespace eyelib {
namespace math {

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

using Mat4 = glm::mat4;
using Quat = glm::quat;

} // namespace math
} // namespace eyelib

