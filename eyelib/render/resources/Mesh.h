#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/AABB.h"
#include "eyelib/assets/AssetId.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <vector>

namespace eyelib {
namespace render {

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
 * @brief Mesh with multiple attributes, submeshes, AABB. GenerateNormals/Tangents, Optimize.
 */
class Mesh {
public:
    Mesh();
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void SetVertices(const std::vector<Vertex>& vertices);
    void SetIndices(const std::vector<uint32_t>& indices);
    void SetIndices(const std::vector<uint16_t>& indices);

    void AddSubmesh(uint32_t indexOffset, uint32_t indexCount, int materialIndex = -1);
    void SetSubmesh(size_t index, uint32_t indexOffset, uint32_t indexCount);
    void ClearSubmeshes();

    void UploadToGPU();
    /** Upload to RHI buffers (for backend-agnostic path). Call with non-null device from pipeline. */
    void UploadToGPU(rhi::IDevice* device);
    void Destroy();
    /** Release RHI buffers if they were created via UploadToGPU(device). Call with same device. */
    void Destroy(rhi::IDevice* device);

    void Bind() const;
    void Unbind() const;
    void Draw(int submeshIndex = -1) const;

    size_t GetVertexCount() const { return m_vertices.size(); }
    size_t GetIndexCount() const { return m_indices32.empty() ? m_indices16.size() : m_indices32.size(); }
    size_t GetSubmeshCount() const { return m_submeshes.size(); }
    const Submesh& GetSubmesh(size_t i) const { return m_submeshes[i]; }

    math::AABB GetAABB() const;
    void ComputeAABB();
    void InvalidateAABB() { m_aabbDirty = true; }

    void GenerateNormals();
    void GenerateTangents();
    void Optimize();

    const std::vector<Vertex>& GetVertices() const { return m_vertices; }
    std::vector<Vertex>& GetVertices() { return m_vertices; }
    const std::vector<uint32_t>& GetIndices32() const { return m_indices32; }
    const std::vector<uint16_t>& GetIndices16() const { return m_indices16; }
    IndexFormat GetIndexFormat() const { return m_indexFormat; }

    bool IsValid() const { return m_vao != 0 || m_vertexBufferRHI != nullptr; }
    unsigned int GetVAO() const { return m_vao; }

    /** RHI path: return vertex/index buffers and layout when UploadToGPU(device) was used. */
    rhi::IBuffer* GetVertexBufferRHI() const { return m_vertexBufferRHI; }
    rhi::IBuffer* GetIndexBufferRHI() const { return m_indexBufferRHI; }
    rhi::IndexType GetRHIIndexType() const;
    static rhi::VertexLayout GetVertexLayoutRHI();

    void SetAssetId(const assets::AssetId& id) { m_assetId = id; }
    const assets::AssetId& GetAssetId() const { return m_assetId; }
    bool HasAssetId() const { return m_assetId.IsValid(); }

private:
    void CreateVAO();
    void Invalidate();

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices32;
    std::vector<uint16_t> m_indices16;
    IndexFormat m_indexFormat = IndexFormat::UInt32;
    std::vector<Submesh> m_submeshes;
    mutable math::AABB m_aabb;
    mutable bool m_aabbDirty = true;

    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_ebo = 0;
    bool m_uploaded = false;

    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IBuffer* m_vertexBufferRHI = nullptr;
    rhi::IBuffer* m_indexBufferRHI = nullptr;

    assets::AssetId m_assetId = assets::AssetId::Invalid();
};

} // namespace render
} // namespace eyelib
