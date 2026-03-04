#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/assets/AssetId.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace render { namespace rhi { struct IDevice; } }
namespace assets {

/** Decoded mesh data (CPU-side). Create Mesh via CreateFromDecoded(device, data). */
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

    /** Load from canonical .oea or .mesh.bin. Device required for GPU mesh. */
    static std::shared_ptr<render::Mesh> LoadFromCanonical(render::rhi::IDevice* device, const std::string& path);

    /** Load all meshes from file. Device required for GPU meshes. */
    static std::vector<std::shared_ptr<render::Mesh>> LoadFromFile(render::rhi::IDevice* device, const std::string& path);

    /** Load single mesh (first from file). Device required. */
    static std::shared_ptr<render::Mesh> LoadSingle(render::rhi::IDevice* device, const std::string& path);

    /** Decode file to CPU data only (no device). For descriptor-only or later CreateFromDecoded. */
    static std::vector<DecodedMeshData> LoadToDecoded(const std::string& path);

    /** Async decode only. Callback runs on worker; create Mesh via CreateFromDecoded(device, data) on main thread. */
    static void LoadAsync(const std::string& path, std::function<void(std::vector<DecodedMeshData>)> callback);

    /** Create GPU Mesh from decoded data. Device required. */
    static std::shared_ptr<render::Mesh> CreateFromDecoded(render::rhi::IDevice* device, const DecodedMeshData& data);

    /** Load mesh and register with AssetManager (uses manager's device). Returns AssetId. */
    static AssetId LoadMesh(const std::string& path);

    /** Load mesh asynchronously and register with AssetManager. */
    static void LoadMeshAsync(const std::string& path, LoadCallback callback);

    /** CPU-only: build and store MeshDescriptor; no GPU upload or instance. For binding phase. */
    static AssetId LoadMeshDescriptorOnly(const std::string& path);
};

} // namespace assets
} // namespace eyelib
