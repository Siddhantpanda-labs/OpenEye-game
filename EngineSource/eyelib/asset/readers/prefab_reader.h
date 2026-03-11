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

/** One node in a prefab hierarchy. meshIndex is set by caller from mesh path -> entry index. */
struct PrefabNode {
    std::string name;
    float localTransform[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::string meshPath;   // e.g. "model_0.mesh"
    int meshIndex = -1;     // index into .omesh mesh entry list; set by caller
    std::vector<PrefabNode> children;
};

/** Reads .prefab file, returns flat list of (mesh path, world transform). Paths are as in file. */
class PrefabReader {
public:
    /** Returns true on success. outDrawables is filled with mesh path + world transform for each entity that has a mesh. */
    static bool Read(const std::string& path, std::vector<PrefabDrawable>& outDrawables);
    static bool ReadFromText(const std::string& text, std::vector<PrefabDrawable>& outDrawables);

    /** Parse prefab text into a hierarchy of nodes (name, local transform, mesh path, children). meshIndex left -1. */
    static bool ReadHierarchyFromText(const std::string& text, std::vector<PrefabNode>& outRoots);
};

} // namespace asset
