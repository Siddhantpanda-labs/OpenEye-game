#pragma once

#include "eyelib/rendering/2d/Font.h"
#include "eyelib/math/Types.h"
#include "eyelib/rendering/rhi/RHI.h"
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace rendering {

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

    void SetDevice(rhi::IDevice* device) { m_injectedDevice = device; }

private:
    struct Vertex {
        math::Vec2 position;
        math::Vec2 uv;
        math::Vec4 color;
    };

    void EnsureShader();
    rhi::VertexLayout GetVertexLayout() const;
    void EnsureBuffers(size_t vertexBytes, size_t indexBytes);

    std::shared_ptr<Shader> m_shader;
    rhi::IDescriptorSet* m_spriteSet = nullptr;
    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;
    rhi::IDevice* m_injectedDevice = nullptr;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IBuffer* m_vertexBufferRHI = nullptr;
    rhi::IBuffer* m_indexBufferRHI = nullptr;
    size_t m_vertexCapacity = 0;
    size_t m_indexCapacity = 0;
    bool m_initialized = false;
};

} // namespace rendering
} // namespace eyelib
