#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/AABB.h"
#include "api/asset/AssetId.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

struct MeshDescriptor;

struct Vertex {
    math::Vec3 position{0, 0, 0};
    math::Vec3 normal{0, 1, 0};
    math::Vec2 uv{0, 0};
    math::Vec2 uv1{0, 0};
    math::Vec3 tangent{0, 0, 0};
    math::Vec4 color{1, 1, 1, 1};
    math::IVec4 boneIndices{0, 0, 0, 0};
    math::Vec4 boneWeights{0, 0, 0, 0};
};

struct Submesh {
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    int materialIndex = -1;
};

enum class IndexFormat { UInt16, UInt32 };

/**
 * GPU-only mesh view for the draw path (Phase 2 CPU vs GPU split).
 * Holds only device buffers and submesh metadata; no CPU geometry.
 * Use Mesh::GetHandle() when Mesh has been uploaded; draw with handle.Bind(device), handle.Draw(device).
 */
struct MeshHandle {
    rhi::IBuffer* vertexBuffer = nullptr;
    rhi::IBuffer* indexBuffer = nullptr;
    rhi::IndexType indexType = rhi::IndexType::UInt32;
    uint32_t totalIndexCount = 0;
    std::vector<Submesh> submeshes;

    bool IsValid() const { return vertexBuffer != nullptr && indexBuffer != nullptr; }
    void Bind(rhi::IDevice* device) const;
    void Draw(rhi::IDevice* device, int submeshIndex = -1) const;
    /** Draw with instance count (e.g. 2 for single-pass stereo). */
    void DrawInstanced(rhi::IDevice* device, int submeshIndex, uint32_t instanceCount) const;
};

/** GPU-only mesh instance (Phase 2.4). No CPU geometry; create via CreateFromDescriptor(device, desc).
 *  CPU data lives in MeshDescriptor / EditableMesh. */
class Mesh {
public:
    Mesh();
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void Destroy();
    /** Release RHI buffers. Call with same device used at creation. */
    void Destroy(rhi::IDevice* device);

    /** Bind vertex/index buffers. Device must be non-null. */
    void Bind(rhi::IDevice* device) const;
    void Unbind() const;
    /** Draw. Device must be non-null. */
    void Draw(rhi::IDevice* device, int submeshIndex = -1) const;

    size_t GetVertexCount() const { return m_vertexCount; }
    size_t GetIndexCount() const { return m_indexCount; }
    size_t GetSubmeshCount() const { return m_submeshes.size(); }
    const Submesh& GetSubmesh(size_t i) const { return m_submeshes[i]; }

    math::AABB GetAABB() const { return m_aabb; }
    IndexFormat GetIndexFormat() const { return m_indexType == rhi::IndexType::UInt16 ? IndexFormat::UInt16 : IndexFormat::UInt32; }

    bool IsValid() const { return m_vertexBufferRHI != nullptr; }
    /** Legacy: returns 0 (no VAO in RHI path). */
    unsigned int GetVAO() const { return 0; }

    rhi::IBuffer* GetVertexBufferRHI() const { return m_vertexBufferRHI; }
    rhi::IBuffer* GetIndexBufferRHI() const { return m_indexBufferRHI; }
    rhi::IndexType GetRHIIndexType() const { return m_indexType; }
    static rhi::VertexLayout GetVertexLayoutRHI();
    /** Fullscreen quad (vec2 pos + vec2 uv). For skybox, post-process, lighting fullscreen. */
    static rhi::VertexLayout GetFullscreenQuadVertexLayoutRHI();

    void SetAssetId(const ::eyelib::api::AssetId& id) { m_assetId = id; }
    const ::eyelib::api::AssetId& GetAssetId() const { return m_assetId; }
    bool HasAssetId() const { return m_assetId.IsValid(); }

    /** Create GPU instance from CPU descriptor. Replaces any existing buffers. */
    bool CreateFromDescriptor(rhi::IDevice* device, const MeshDescriptor& desc);
    /** Factory: create a new GPU mesh from descriptor. */
    static std::shared_ptr<Mesh> Create(rhi::IDevice* device, const MeshDescriptor& desc);

    /** GPU-only view for draw path. Valid only after CreateFromDescriptor. */
    MeshHandle GetHandle() const;

private:
    size_t m_vertexCount = 0;
    size_t m_indexCount = 0;
    rhi::IndexType m_indexType = rhi::IndexType::UInt32;
    std::vector<Submesh> m_submeshes;
    math::AABB m_aabb;

    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IBuffer* m_vertexBufferRHI = nullptr;
    rhi::IBuffer* m_indexBufferRHI = nullptr;

    ::eyelib::api::AssetId m_assetId = ::eyelib::api::AssetId::Invalid();
};

} // namespace render
} // namespace eyelib
