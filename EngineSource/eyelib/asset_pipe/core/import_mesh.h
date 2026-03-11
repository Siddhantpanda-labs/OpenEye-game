#pragma once

#include "asset_pipe/core/math_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace asset_pipe {

struct ImportVertex {
    Vec3 position{};
    Vec3 normal{};
    Vec4 tangent{};
    Vec2 uv0{};
    Vec2 uv1{};
};

struct ImportSubmesh {
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

struct ImportMesh {
    std::string name;

    std::vector<ImportVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<ImportSubmesh> submeshes;

    AABB bounds{};
};

} // namespace asset_pipe
