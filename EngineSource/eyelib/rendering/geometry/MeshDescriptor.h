#pragma once

#include "eyelib/rendering/resources/Mesh.h"
#include <vector>

namespace eyelib {
namespace rendering {

/**
 * CPU-only mesh definition (Phase 3: asset vs GPU instance).
 * Vertices, indices, submeshes; no RHI buffers. Safe to store in registry and serialize.
 * Use with Mesh::CreateFromDescriptor(device, desc) to create a GPU instance.
 */
struct MeshDescriptor {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices32;
    std::vector<uint16_t> indices16;
    IndexFormat indexFormat = IndexFormat::UInt32;
    std::vector<Submesh> submeshes;
};

} // namespace rendering
} // namespace eyelib
