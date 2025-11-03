#pragma once

#include "../Scene.h"
#include <glm/glm.hpp>

namespace OpenEye {
namespace ECS {

// LOD system - updates LOD levels based on camera distance
class LODSystem {
public:
    // Update LOD for all entities with LOD component
    static void Update(Scene& scene, const glm::vec3& cameraPosition);
};

} // namespace ECS
} // namespace OpenEye

