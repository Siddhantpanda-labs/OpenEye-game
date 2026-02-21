#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/ShadowMap.h"
#include "eyelib/render/rhi/RHI.h"
#include <functional>
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

    void SetDevice(rhi::IDevice* device) { m_device = device; }
    void SetDepthShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetDepthShader() const { return m_depthShader; }

    /** cmd is used for ShadowMap bind/unbind (SetRenderTarget, Clear). May be null if shadow map uses legacy path. */
    void Render(ShadowMap& shadowMap,
                rhi::ICommandList* cmd,
                const std::vector<Drawable>& drawables,
                const math::Vec3& lightDir,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& cameraPos,
                float nearPlane,
                float farPlane);

    using DrawableFeed = std::function<void(std::function<void(const Drawable&)>)>;
    void Render(ShadowMap& shadowMap,
                rhi::ICommandList* cmd,
                DrawableFeed feedDrawables,
                const math::Vec3& lightDir,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& cameraPos,
                float nearPlane,
                float farPlane);

    ShadowMap* GetShadowMap() { return m_currentShadowMap; }

    /** Reload depth shaders (for hot-reload when shader files change). */
    void ReloadShaders();

private:
    void EnsureDefaultShader();
    void EnsureSkinnedShader();
    void RenderDrawables(ShadowMap& shadowMap,
                        rhi::ICommandList* cmd,
                        DrawableFeed feedDrawables,
                        const math::Vec3& lightDir,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Vec3& cameraPos,
                        float nearPlane,
                        float farPlane);

    std::shared_ptr<Shader> m_depthShader;
    std::shared_ptr<Shader> m_depthSkinnedShader;
    ShadowMap* m_currentShadowMap = nullptr;
    rhi::IDevice* m_device = nullptr;
};

} // namespace render
} // namespace eyelib
