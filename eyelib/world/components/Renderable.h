#pragma once

#include <string>
#include <vector>

namespace eyelib {
namespace world {
namespace components {

struct Renderable {
    std::string meshAssetId;
    std::string materialAssetId;
    bool visible = true;
    std::vector<std::string> lodMeshAssetIds;
    size_t activeLOD = 0;
};

} // namespace components
} // namespace world
} // namespace eyelib
