#pragma once

#include "eyelib/math/AABB.h"
#include <cstddef>
#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Public PIMPL handle to a GPU mesh (vertex + index buffers + submesh metadata).
 *
 * Opaque to clients: the underlying internal type (`eyelib::rendering::Mesh`) is never
 * named in this header. Clients copy/move/share this handle freely; the GPU buffers
 * live as long as any handle references them.
 *
 * Construction is **not exposed** in M1. The factory entry point that produces
 * `Mesh` instances (e.g. from a `MeshDescriptor` POD, or by loading an asset by id)
 * is designed in M3 (editor `GPUResourceCache` migration) so the shape can be driven
 * by a concrete consumer rather than guessed in advance. Until then, internal code
 * inside `api/rendering/*.cpp` constructs handles via the friend-only seam in
 * `api/rendering/internal/ResourceAccess.h`.
 *
 * Field semantics:
 *  - `GetMeshHandle()` returns a uint32 token consumed by `DrawableInstance::meshHandle`.
 *    A handle table maps it back to the internal `MeshHandle` via the mesh resolver
 *    bound to the pipeline (M3 wires the resolver).
 *  - Returns `0` (invalid) for a default-constructed handle.
 *
 * Thread safety: copy/move are safe; observation methods (`IsValid`, `Get*`) are safe
 * for concurrent reads. Mutating the underlying GPU mesh is the factory's responsibility.
 */
class Mesh {
public:
    Mesh() noexcept;
    ~Mesh();

    Mesh(const Mesh& other);
    Mesh& operator=(const Mesh& other);
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    /** True iff the handle refers to a GPU-valid mesh (vertex + index buffers uploaded). */
    bool IsValid() const noexcept;

    /** Token for `DrawableInstance::meshHandle`. Zero when invalid. */
    uint32_t GetMeshHandle() const noexcept;

    /** Submesh count; 0 when invalid. */
    size_t GetSubmeshCount() const noexcept;

    /** Total vertex / index counts (across all submeshes); 0 when invalid. */
    size_t GetVertexCount() const noexcept;
    size_t GetIndexCount() const noexcept;

    /** Model-space AABB; default-constructed when invalid. */
    math::AABB GetAABB() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    // Friend-only construction seam (see api/rendering/internal/ResourceAccess.h).
    friend class MeshAccess;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
