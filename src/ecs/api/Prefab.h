#pragma once

#include "../Prefab.h"

namespace OpenEye {
namespace ECS {
namespace API {

/**
 * @brief Public API for Prefabs
 * 
 * Re-exports Prefab and PrefabManager from the ECS component.
 */
using Prefab = OpenEye::ECS::Prefab;
using PrefabManager = OpenEye::ECS::PrefabManager;

} // namespace API
} // namespace ECS
} // namespace OpenEye

