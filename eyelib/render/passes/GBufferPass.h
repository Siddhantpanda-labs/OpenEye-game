#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/GBuffer.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

class Shader;

/** Renders geometry to G-buffer (position, normal, albedo, PBR). */
class GBufferPass {
public:
    GBufferPass();
    ~GBufferPass() = default;

    void SetGeometryShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetGeometryShader() const { return m_geometryShader; }

    /** Render drawables into the G-buffer. Uses default embedded shader if none set. */
    void Render(GBuffer& gbuffer,
                const std::vector<Drawable>& drawables,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Mat4& prevView,
                const math::Mat4& prevProjection);

    GBuffer* GetGBuffer() { return m_currentGBuffer; }

private:
    void EnsureDefaultShader();

    std::shared_ptr<Shader> m_geometryShader;
    GBuffer* m_currentGBuffer = nullptr;
};

} // namespace render
} // namespace eyelib
