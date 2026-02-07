#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/GBuffer.h"
#include "eyelib/render/rhi/RHI.h"
#include <memory>
#include <span>
#include <vector>

namespace eyelib {
namespace api {
struct Drawable;
}
namespace render {

class Shader;

/** Renders geometry to G-buffer (position, normal, albedo, PBR). */
class GBufferPass {
public:
    GBufferPass();
    ~GBufferPass() = default;

    void SetGeometryShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetGeometryShader() const { return m_geometryShader; }

    /** Render drawables into the G-buffer. cmd used for depth/cull state (GL-free). */
    void Render(GBuffer& gbuffer,
                rhi::ICommandList* cmd,
                const std::vector<Drawable>& drawables,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Mat4& prevView,
                const math::Mat4& prevProjection);

    /** Zero-copy: render from API drawables. cmd used for depth/cull state. */
    void Render(GBuffer& gbuffer,
                rhi::ICommandList* cmd,
                std::span<const api::Drawable> drawables,
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
