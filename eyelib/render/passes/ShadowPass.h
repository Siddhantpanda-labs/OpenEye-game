#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/ShadowMap.h"
#include <memory>
#include <span>
#include <vector>

namespace eyelib {
namespace api {
struct Drawable;
}
namespace render {

class Shader;

/** Renders depth from light POV for directional shadow mapping. */
class ShadowPass {
public:
    ShadowPass();
    ~ShadowPass() = default;

    void SetDepthShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetDepthShader() const { return m_depthShader; }

    void Render(ShadowMap& shadowMap,
                const std::vector<Drawable>& drawables,
                const math::Vec3& lightDir,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& cameraPos,
                float nearPlane,
                float farPlane);

    /** Zero-copy: render from API drawables (resolves GetInternal() at use). Accepts any contiguous container via span. */
    void Render(ShadowMap& shadowMap,
                std::span<const api::Drawable> drawables,
                const math::Vec3& lightDir,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& cameraPos,
                float nearPlane,
                float farPlane);

    ShadowMap* GetShadowMap() { return m_currentShadowMap; }

private:
    void EnsureDefaultShader();
    void EnsureSkinnedShader();

    std::shared_ptr<Shader> m_depthShader;
    std::shared_ptr<Shader> m_depthSkinnedShader;
    ShadowMap* m_currentShadowMap = nullptr;
};

} // namespace render
} // namespace eyelib
