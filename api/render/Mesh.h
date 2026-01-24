#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/AABB.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace eyelib {
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
    };

    Mesh();
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    /** Set vertex data. */
    void SetVertices(const std::vector<Vertex>& vertices);

    /** Set index data (32-bit). */
    void SetIndices(const std::vector<uint32_t>& indices);

    /** Upload to GPU. Call after setting vertices/indices. */
    void UploadToGPU();

    /** Get vertex count. */
    size_t GetVertexCount() const;

    /** Get index count. */
    size_t GetIndexCount() const;

    /** Get AABB. */
    math::AABB GetAABB() const;

    /** Check if valid. */
    bool IsValid() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
