#pragma once

#include "ComponentHandles.h"

#include <vector>

namespace eyelib {
namespace ecs {
namespace components {

struct Renderable {
    MeshHandle mesh;
    MaterialHandle material;
    bool visible = true;
    std::vector<MeshHandle> lodMeshes;
    size_t activeLOD = 0;
};

} // namespace components
} // namespace ecs
} // namespace eyelib
