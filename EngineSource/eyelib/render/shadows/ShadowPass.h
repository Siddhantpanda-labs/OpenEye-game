#pragma once

#include "eyelib/render/RenderContext.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/rhi/RHI.h"
#include <functional>
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

class Mesh;
class Material;
class Shader;
class ShaderCache;

/** Renders depth from light POV for directional cascade shadow mapping. Requires SetShaderCache; shaders come from cache (MaterialCompiler) only. */
class ShadowPass {
public:
    ShadowPass();
    ~ShadowPass() = default;

    /** Required: shadow pass uses key-driven shader cache (PipelineStage::Depth) and MaterialCompiler variants. */
    void SetShaderCache(ShaderCache* cache) { m_shaderCache = cache; }

    using DrawableFeed = std::function<void(std::function<void(const Drawable&)>)>;

    /** Cascade shadow map: updates cascade, then for each cascade binds layer, sets light matrix, draws. */
    void Render(CascadeShadowMap& cascadeMap,
                rhi::IDevice* device,
                rhi::ICommandList* cmd,
                DrawableFeed feedDrawables,
                const math::Vec3& lightDir,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& cameraPos,
                float nearPlane,
                float farPlane,
                float aspectRatio = 0.0f);

    /**
     * Instanced draw (7.3): one mesh + material, N instances for cascade shadow.
     * instanceMatrixBuffer must contain instanceCount mat4s (column-major).
     */
    void RenderInstanced(CascadeShadowMap& cascadeMap,
                         rhi::IDevice* device,
                         rhi::ICommandList* cmd,
                         const Mesh* mesh,
                         const Material* material,
                         rhi::IBuffer* instanceMatrixBuffer,
                         uint32_t instanceCount,
                         const math::Vec3& lightDir,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Vec3& cameraPos,
                         float nearPlane,
                         float farPlane,
                         float aspectRatio = 0.0f);

    /** No-op; cache owns shaders and is reloaded at pipeline level. */
    void ReloadShaders() {}

private:
    ShaderCache* m_shaderCache = nullptr;
};

} // namespace render
} // namespace eyelib
