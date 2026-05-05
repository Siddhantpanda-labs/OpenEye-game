#pragma once

#include <string>

namespace eyelib {
namespace api {
namespace geometry {

/** Prefix for built-in procedural mesh ids. Used by editor and runtime scene builders. */
constexpr const char* kBuiltinMeshPrefix = "builtin://";

/** Built-in primitive mesh identifiers. Use as meshAssetId on Renderable/MeshRenderer. */
namespace BuiltinMesh {
constexpr const char* Cube = "builtin://Cube";
constexpr const char* Sphere = "builtin://Sphere";
constexpr const char* Plane = "builtin://Plane";
constexpr const char* Cylinder = "builtin://Cylinder";
constexpr const char* Cone = "builtin://Cone";
constexpr const char* Torus = "builtin://Torus";
} // namespace BuiltinMesh

/** True if \a meshId is a built-in primitive (builtin://...). */
inline bool IsBuiltinMeshId(const std::string& meshId) {
    if (meshId.empty()) return false;
    const char* p = kBuiltinMeshPrefix;
    for (; *p && meshId.size() > static_cast<size_t>(p - kBuiltinMeshPrefix); ++p) {
        if (meshId[p - kBuiltinMeshPrefix] != *p) return false;
    }
    return *p == '\0';
}

} // namespace geometry
} // namespace api
} // namespace eyelib
