#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/assets/AssetId.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace assets {

/** Decoded mesh data (CPU-side). Create Mesh on main thread via CreateFromDecoded. */
struct DecodedMeshData {
    std::vector<render::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<render::Submesh> submeshes;
    std::string name;
};

/**
 * @brief Mesh loading via Assimp (interchange) and canonical .mesh.bin (runtime).
 */
class MeshLoader {
public:
    using LoadCallback = std::function<void(AssetId)>;

    /** Import interchange (GLTF/FBX/etc) to canonical .oea. Exports first mesh only. Returns true on success. */
    static bool ImportToCanonical(const std::string& interchangePath, const std::string& outputPath);

    /** Load from canonical .oea or .mesh.bin (Mesh Pipeline). Returns single mesh or nullptr. */
    static std::shared_ptr<render::Mesh> LoadFromCanonical(const std::string& path);

    /** Load all meshes from file. Supports OBJ, GLTF, FBX, DAE. */
    static std::vector<std::shared_ptr<render::Mesh>> LoadFromFile(const std::string& path);

    /** Load single mesh (first from file). */
    static std::shared_ptr<render::Mesh> LoadSingle(const std::string& path);

    /** Async decode only. Callback runs on worker; create Mesh via CreateFromDecoded on main thread. */
    static void LoadAsync(const std::string& path, std::function<void(std::vector<DecodedMeshData>)> callback);

    /** Create Mesh from decoded data. Call on main thread (uses GL). */
    static std::shared_ptr<render::Mesh> CreateFromDecoded(const DecodedMeshData& data);

    /** Load mesh and register with AssetManager. Returns AssetId. */
    static AssetId LoadMesh(const std::string& path);

    /** Load mesh asynchronously and register with AssetManager. */
    static void LoadMeshAsync(const std::string& path, LoadCallback callback);
};

} // namespace assets
} // namespace eyelib
