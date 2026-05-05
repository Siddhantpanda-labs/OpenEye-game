#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace world {
namespace components {

struct Renderable {
    std::string meshAssetId;
    std::string materialAssetId;
    bool visible = true;
    /** Index into the mesh's submeshes (0 = first). When mesh is multi-submesh (e.g. .omesh), only this submesh is drawn. -1 = draw all. */
    int32_t submeshIndex = -1;
    std::vector<std::string> lodMeshAssetIds;
    size_t activeLOD = 0;
};

} // namespace components
} // namespace world
} // namespace eyelib
