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
                         float aspectRatio = 0.0f,
                         int32_t submeshIndex = -1);

    /** No-op; cache owns shaders and is reloaded at pipeline level. */
    void ReloadShaders() {}

    /** Releases resource-space descriptor sets and UBO. Call before device teardown. */
    void Shutdown();

private:
    void EnsureResourceSpaceSets(rhi::IDevice* device);

    ShaderCache* m_shaderCache = nullptr;
    rhi::IDevice* m_device = nullptr;
    rhi::IDescriptorSet* m_frameSet = nullptr;
    rhi::IDescriptorSet* m_shadowPassSet = nullptr;
    rhi::IDescriptorSet* m_sceneBuffersSet = nullptr;
    rhi::IBuffer* m_frameUBO = nullptr;
    rhi::IBuffer* m_bonesBuffer = nullptr;  // SceneBuffers binding 1 for skinned (resource-space)
    rhi::IBuffer* m_sceneFallbackBuffer = nullptr;  // SceneBuffers fallback for required bindings 0..3
};

} // namespace render
} // namespace eyelib
