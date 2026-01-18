#pragma once

#include "../Scene.h"

namespace OpenEye {
namespace ECS {
namespace API {

/**
 * @brief Public API for Scene management
 * 
 * This re-exports the Scene class from the ECS component.
 * Apps should include this instead of ecs/Scene.h directly.
 */
using Scene = OpenEye::ECS::Scene;
using Entity = OpenEye::ECS::Entity;

} // namespace API
} // namespace ECS
} // namespace OpenEye

