#pragma once

#include <string>

namespace eyelib {
namespace world {
namespace components {

struct MeshRenderer {
    std::string meshAssetId;
    std::string materialAssetId;
    bool visible = true;
};

} // namespace components
} // namespace world
} // namespace eyelib
