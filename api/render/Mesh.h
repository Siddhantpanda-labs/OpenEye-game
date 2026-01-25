#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/AABB.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace eyelib {
namespace render { class Mesh; }
namespace api {

/**
 * @brief Mesh handle (opaque). Set vertices/indices, upload to GPU, draw.
 * No OpenGL types exposed.
 */
class Mesh {
public:
    struct Vertex {
        math::Vec3 position{0, 0, 0};
        math::Vec3 normal{0, 1, 0};
        math::Vec2 uv{0, 0};
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

    Mesh();
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    /** Set vertex data. */
    void SetVertices(const std::vector<Vertex>& vertices);

    /** Set index data. */
    void SetIndices(const std::vector<uint32_t>& indices);
    void SetIndices(const std::vector<uint16_t>& indices);

    void AddSubmesh(uint32_t indexOffset, uint32_t indexCount, int materialIndex = -1);
    void SetSubmesh(size_t index, uint32_t indexOffset, uint32_t indexCount);
    void ClearSubmeshes();

    /** Upload to GPU. Call after setting vertices/indices. */
    void UploadToGPU();

    /** Get vertex count. */
    size_t GetVertexCount() const;

    /** Get index count. */
    size_t GetIndexCount() const;

    /** Get AABB. */
    math::AABB GetAABB() const;
    size_t GetSubmeshCount() const;
    Submesh GetSubmesh(size_t index) const;
    IndexFormat GetIndexFormat() const;

    void GenerateNormals();
    void GenerateTangents();
    void Optimize();

    /** Check if valid. */
    bool IsValid() const;

    static std::shared_ptr<Mesh> WrapInternal(const std::shared_ptr<::eyelib::render::Mesh>& mesh);
    std::shared_ptr<::eyelib::render::Mesh> GetInternal() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
