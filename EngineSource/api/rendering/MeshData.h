#pragma once

#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Public POD vertex layout for `MeshData` upload.
 *
 * Field-for-field mirror of the internal `eyelib::rendering::Vertex`. Documented as
 * stable public ABI: this layout is what the editor / player produces to feed
 * `ResourceCache::CreateMesh`. Translation to the internal vertex (and to GPU buffers)
 * happens inside `ResourceCache.cpp` — the only TU outside `eyelib/rendering/` allowed
 * to name the internal type.
 *
 * Skinning fields (`boneIndices`, `boneWeights`) are reserved for the skinned-mesh
 * pipeline (M4.3); static meshes leave them at their defaults.
 */
struct Vertex {
    math::Vec3  position{0.0f, 0.0f, 0.0f};
    math::Vec3  normal{0.0f, 1.0f, 0.0f};
    math::Vec2  uv{0.0f, 0.0f};
    math::Vec2  uv1{0.0f, 0.0f};
    math::Vec3  tangent{0.0f, 0.0f, 0.0f};
    math::Vec4  color{1.0f, 1.0f, 1.0f, 1.0f};
    math::IVec4 boneIndices{0, 0, 0, 0};
    math::Vec4  boneWeights{0.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * @brief Submesh range + material slot. Mirrors `eyelib::rendering::Submesh`.
 *
 * `materialIndex` is the per-submesh material slot used by the GBuffer pass to override
 * the per-instance `materialId` (matches the new module's `DrawItem` layout). Set to
 * `-1` to fall back to the per-instance material.
 */
struct Submesh {
    uint32_t indexOffset   = 0;
    uint32_t indexCount    = 0;
    int      materialIndex = -1;
};

/** Index width for the mesh's index buffer. Mirrors `eyelib::rendering::IndexFormat`. */
enum class IndexFormat { UInt16, UInt32 };

/**
 * @brief Public CPU-only mesh definition consumed by `ResourceCache::CreateMesh`.
 *
 * Vertices, indices, and submeshes — no GPU resources. The cache uploads to the
 * device via the new module's `eyelib::rendering::Mesh::Create` factory and returns
 * an opaque `Mesh` handle.
 *
 * Fill exactly one of `indices32` / `indices16` — the matching one is selected by
 * `indexFormat`. `localBounds` is optional: if it stays default-constructed (zero
 * AABB), the cache derives it from `vertices` during upload.
 *
 * Safe to copy / move / store. POD.
 */
struct MeshData {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices32;
    std::vector<uint16_t> indices16;
    IndexFormat           indexFormat = IndexFormat::UInt32;
    std::vector<Submesh>  submeshes;
    math::AABB            localBounds{};
};

}  // namespace rendering
}  // namespace api
}  // namespace eyelib
