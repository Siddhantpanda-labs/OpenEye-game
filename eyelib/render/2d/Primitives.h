#pragma once

#include "eyelib/math/Types.h"
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

    void SetViewProjection(const math::Mat4& view, const math::Mat4& projection);

    void DrawLine(const math::Vec2& start, const math::Vec2& end, const math::Vec4& color, float width = 1.0f);
    void DrawCircle(const math::Vec2& center, float radius, const math::Vec4& color, int segments = 32);
    void DrawRectangle(const math::Vec2& min, const math::Vec2& max, const math::Vec4& color);
    void DrawPolygon(const std::vector<math::Vec2>& points, const math::Vec4& color);

    /** Submit batched primitives to GPU. Call after Draw* calls. */
    void Flush();

private:
    void FlushLines();
    void FlushTriangles();

    struct LineVert { math::Vec2 pos; math::Vec4 color; };
    struct TriVert { math::Vec2 pos; math::Vec4 color; };
    std::vector<LineVert> m_lineVerts;
    std::vector<unsigned int> m_lineIndices;
    std::vector<TriVert> m_triVerts;
    std::vector<unsigned int> m_triIndices;
    math::Mat4 m_view{1.0f};
    math::Mat4 m_projection{1.0f};
    unsigned int m_vao = 0;
    unsigned int m_lineVbo = 0;
    unsigned int m_lineEbo = 0;
    unsigned int m_triVbo = 0;
    unsigned int m_triEbo = 0;
    bool m_initialized = false;
};

} // namespace render
} // namespace eyelib
