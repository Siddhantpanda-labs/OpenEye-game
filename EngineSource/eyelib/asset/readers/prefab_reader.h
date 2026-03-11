#pragma once

#include "asset/core/math_types.h"

#include <string>
#include <vector>

namespace asset {

/** One drawable from a prefab: mesh path (relative to prefab dir) + world transform. */
struct PrefabDrawable {
    std::string meshPath;  // e.g. "house_object_0.mesh"
    Mat4 worldTransform;
};

/** Reads .prefab file, returns flat list of (mesh path, world transform). Paths are as in file. */
class PrefabReader {
public:
    /** Returns true on success. outDrawables is filled with mesh path + world transform for each entity that has a mesh. */
    static bool Read(const std::string& path, std::vector<PrefabDrawable>& outDrawables);
    static bool ReadFromText(const std::string& text, std::vector<PrefabDrawable>& outDrawables);
};

} // namespace asset
