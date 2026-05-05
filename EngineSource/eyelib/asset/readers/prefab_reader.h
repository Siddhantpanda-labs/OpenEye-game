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

/** One node in a prefab hierarchy. meshIndex is a stable mesh-entry index when present. */
struct PrefabNode {
    std::string name;
    float localTransform[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::string meshPath;   // e.g. "model_0.mesh"
    int meshIndex = -1;     // mesh entry index from prefab (preferred); caller may remap to submesh start
    std::vector<PrefabNode> children;
};

/** Reads .prefab file, returns flat list of (mesh path, world transform). Paths are as in file. */
class PrefabReader {
public:
    /** Returns true on success. outDrawables is filled with mesh path + world transform for each entity that has a mesh. */
    static bool Read(const std::string& path, std::vector<PrefabDrawable>& outDrawables);
    static bool ReadFromText(const std::string& text, std::vector<PrefabDrawable>& outDrawables);

    /** Parse prefab text into a hierarchy of nodes (name, local transform, mesh path, optional meshIndex, children). */
    static bool ReadHierarchyFromText(const std::string& text, std::vector<PrefabNode>& outRoots);
};

} // namespace asset
