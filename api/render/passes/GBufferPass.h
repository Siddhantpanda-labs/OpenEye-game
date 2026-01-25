#pragma once

#include "api/render/GBuffer.h"
#include "api/render/RenderScene.h"
#include "api/render/Shader.h"
#include "eyelib/math/Types.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace render { class GBufferPass; }
namespace api {

class GBufferPass {
public:
    GBufferPass();
    ~GBufferPass();

    GBufferPass(const GBufferPass&) = delete;
    GBufferPass& operator=(const GBufferPass&) = delete;
    GBufferPass(GBufferPass&& other) noexcept;
    GBufferPass& operator=(GBufferPass&& other) noexcept;

    void SetGeometryShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetGeometryShader() const;

    void Render(GBuffer& gbuffer,
                const std::vector<Drawable>& drawables,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Mat4& prevView,
                const math::Mat4& prevProjection);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
