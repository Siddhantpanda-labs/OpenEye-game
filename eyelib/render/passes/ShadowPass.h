#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/ShadowMap.h"
#include <memory>
#include <vector>

namespace eyelib {
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

    ShadowMap* GetShadowMap() { return m_currentShadowMap; }

private:
    void EnsureDefaultShader();

    std::shared_ptr<Shader> m_depthShader;
    ShadowMap* m_currentShadowMap = nullptr;
};

} // namespace render
} // namespace eyelib
