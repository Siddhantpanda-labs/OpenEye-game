#pragma once

#include "asset/core/math_types.h"

#include <string>
#include <vector>

namespace asset {

/** Node in the scene hierarchy. meshIndex is -1 if no mesh. */
struct ImportNode {
    std::string name;
    Mat4 localTransform{};

    int meshIndex = -1;  // index into ImportScene::meshes
    std::vector<int> children;  // indices into flat node array or child nodes
};

} // namespace asset
