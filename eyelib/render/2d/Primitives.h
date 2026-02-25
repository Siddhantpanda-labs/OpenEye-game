#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"
#include <vector>

namespace eyelib {
namespace render {

/**
 * @brief 2D primitives: lines, circles, rectangles, polygons.
 * Draw* functions render immediately. Set view/projection before drawing.
 */
class Primitives {
public:
    Primitives();
    ~Primitives();

    Primitives(const Primitives&) = delete;
    Primitives& operator=(const Primitives&) = delete;

    bool Initialize();
    void Shutdown();
    /** Release static shader cache. Call before destroying the device they were created with. */
    static void ShutdownStaticResources();

    void SetViewProjection(const math::Mat4& view, const math::Mat4& projection);

    void DrawLine(const math::Vec2& start, const math::Vec2& end, const math::Vec4& color, float width = 1.0f);
    void DrawCircle(const math::Vec2& center, float radius, const math::Vec4& color, int segments = 32);
    void DrawRectangle(const math::Vec2& min, const math::Vec2& max, const math::Vec4& color);
    void DrawPolygon(const std::vector<math::Vec2>& points, const math::Vec4& color);

    /** Submit batched primitives to GPU. Call after Draw* calls. */
    void Flush();

    void SetDevice(rhi::IDevice* device) { m_injectedDevice = device; }

private:
    void FlushLines();
    void FlushTriangles();
    rhi::VertexLayout GetVertexLayout() const;
    void EnsureLineBuffers(size_t vertexBytes, size_t indexBytes);
    void EnsureTriBuffers(size_t vertexBytes, size_t indexBytes);

    struct LineVert { math::Vec2 pos; math::Vec4 color; };
    struct TriVert { math::Vec2 pos; math::Vec4 color; };
    std::vector<LineVert> m_lineVerts;
    std::vector<unsigned int> m_lineIndices;
    std::vector<TriVert> m_triVerts;
    std::vector<unsigned int> m_triIndices;
    math::Mat4 m_view{1.0f};
    math::Mat4 m_projection{1.0f};
    rhi::IDevice* m_injectedDevice = nullptr;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IBuffer* m_lineVbo = nullptr;
    rhi::IBuffer* m_lineEbo = nullptr;
    rhi::IBuffer* m_triVbo = nullptr;
    rhi::IBuffer* m_triEbo = nullptr;
    size_t m_lineVboCapacity = 0;
    size_t m_lineEboCapacity = 0;
    size_t m_triVboCapacity = 0;
    size_t m_triEboCapacity = 0;
    bool m_initialized = false;
};

} // namespace render
} // namespace eyelib
