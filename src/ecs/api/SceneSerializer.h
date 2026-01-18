#pragma once

#include "../SceneSerializer.h"

namespace OpenEye {
namespace ECS {
namespace API {

/**
 * @brief Public API for Scene Serialization
 * 
 * Re-exports SceneSerializer from the ECS component.
 */
using SceneSerializer = OpenEye::ECS::SceneSerializer;

} // namespace API
} // namespace ECS
} // namespace OpenEye

