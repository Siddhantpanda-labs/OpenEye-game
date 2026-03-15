#pragma once

#include "eyelib/render/2d/Sprite.h"
#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

class Shader;
class Texture2D;

/**
 * @brief Batches and renders 2D sprites.
 * AddSprite to batch, Render(view, projection) to draw.
 * Supports sprite sheets via UV rect; texture atlas via same texture + different UVs.
 */
class SpriteRenderer {
public:
    SpriteRenderer();
    ~SpriteRenderer();

    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    /** Initialize (load default shader, create buffers). */
    bool Initialize();
    void Shutdown();

    void AddSprite(const Sprite& sprite);
    void Clear();
    void Render(const math::Mat4& view, const math::Mat4& projection);

    /** Use custom shader (must have u_Model, u_View, u_Projection, u_Texture, a_Position, a_UV, a_Color). */
    void SetShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetShader() const { return m_shader; }

    void SetDevice(rhi::IDevice* device) { m_injectedDevice = device; }

private:
    struct Vertex {
        math::Vec2 position;
        math::Vec2 uv;
        math::Vec4 color;
    };
    struct Batch {
        std::shared_ptr<Texture2D> texture;
        size_t indexOffset;
        size_t indexCount;
    };

    void EnsureShader();
    void BuildBatches();
    rhi::VertexLayout GetVertexLayout() const;
    void EnsureBuffers(size_t vertexBytes, size_t indexBytes);

    std::shared_ptr<Shader> m_shader;
    rhi::IDescriptorSet* m_spriteSet = nullptr;
    std::vector<Sprite> m_sprites;
    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;
    std::vector<Batch> m_batches;
    rhi::IDevice* m_injectedDevice = nullptr;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IBuffer* m_vertexBufferRHI = nullptr;
    rhi::IBuffer* m_indexBufferRHI = nullptr;
    size_t m_vertexCapacity = 0;
    size_t m_indexCapacity = 0;
    bool m_initialized = false;
    bool m_dirty = true;
};

} // namespace render
} // namespace eyelib
