#pragma once

#include "../Scene.h"

namespace OpenEye {
namespace ECS {
namespace API {

/**
 * @brief Entity handle type
 * 
 * Re-exports Entity type from ECS component.
 * Apps should use this instead of accessing entt directly.
 */
using Entity = OpenEye::ECS::Entity;
static constexpr Entity NullEntity = OpenEye::ECS::Entity{};

} // namespace API
} // namespace ECS
} // namespace OpenEye

