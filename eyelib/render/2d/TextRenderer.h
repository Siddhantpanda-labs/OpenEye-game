#pragma once

#include "eyelib/render/2d/Font.h"
#include "eyelib/math/Types.h"
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace render {

class Shader;

/**
 * @brief Renders text using a Font. Glyph caching via Font atlas.
 * RenderText(text, font, position, color, size, view, projection) draws immediately.
 */
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    bool Initialize();
    void Shutdown();

    /** Draw text. Position = bottom-left in 2D space. Uses font atlas. */
    void RenderText(const std::string& text, Font& font, const math::Vec2& position,
                    const math::Vec4& color, float size,
                    const math::Mat4& view, const math::Mat4& projection);

    void SetShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetShader() const { return m_shader; }

private:
    struct Vertex {
        math::Vec2 position;
        math::Vec2 uv;
        math::Vec4 color;
    };

    void EnsureShader();

    std::shared_ptr<Shader> m_shader;
    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_ebo = 0;
    bool m_initialized = false;
};

} // namespace render
} // namespace eyelib
