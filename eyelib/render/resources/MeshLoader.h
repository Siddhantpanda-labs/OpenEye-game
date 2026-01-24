#pragma once

#include "eyelib/render/resources/Mesh.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace render {

/** Decoded mesh data (CPU-side). Create Mesh on main thread via CreateFromDecoded. */
struct DecodedMeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh> submeshes;
    std::string name;
};

/**
 * @brief Mesh loading via Assimp. OBJ, GLTF, FBX, DAE, etc.
 */
class MeshLoader {
public:
    /** Load all meshes from file. Supports OBJ, GLTF, FBX, DAE. */
    static std::vector<std::shared_ptr<Mesh>> LoadFromFile(const std::string& path);

    /** Load single mesh (first from file). */
    static std::shared_ptr<Mesh> LoadSingle(const std::string& path);

    /** Async decode only. Callback runs on worker; create Mesh via CreateFromDecoded on main thread. */
    static void LoadAsync(const std::string& path, std::function<void(std::vector<DecodedMeshData>)> callback);

    /** Create Mesh from decoded data. Call on main thread (uses GL). */
    static std::shared_ptr<Mesh> CreateFromDecoded(const DecodedMeshData& data);
};

} // namespace render
} // namespace eyelib
