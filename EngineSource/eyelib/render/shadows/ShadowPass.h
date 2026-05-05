#pragma once

#include "eyelib/render/RenderContext.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/rhi/RHI.h"
#include <array>
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
    void ResetTransientFrameStateIfNeeded(rhi::IDevice* device);
    rhi::IDescriptorSet* AcquirePerDrawSceneSet(rhi::IDevice* device, uint32_t frameIndex);
    rhi::IDescriptorSet* BuildSkinnedSceneSet(rhi::IDevice* device, uint32_t frameIndex, const math::Mat4* bonePtr, size_t boneCount);
    rhi::IDescriptorSet* BuildInstancedSceneSet(rhi::IDevice* device, uint32_t frameIndex, rhi::IBuffer* instanceMatrixBuffer, uint32_t instanceCount);

    ShaderCache* m_shaderCache = nullptr;
    rhi::IDevice* m_device = nullptr;
    rhi::IDescriptorSet* m_frameSet = nullptr;
    rhi::IDescriptorSet* m_shadowPassSet = nullptr;
    rhi::IDescriptorSet* m_sceneBuffersSet = nullptr;
    rhi::IBuffer* m_frameUBO = nullptr;
    rhi::IBuffer* m_bonesBuffer = nullptr;  // SceneBuffers binding 1 for skinned (resource-space)
    rhi::IBuffer* m_sceneFallbackBuffer = nullptr;  // SceneBuffers fallback for required bindings 0..3
    static constexpr uint32_t kMaxFramesInFlight = 3u;
    static constexpr uint32_t kMaxSkinnedDrawsPerFrame = 256u;
    std::array<std::vector<rhi::IDescriptorSet*>, kMaxFramesInFlight> m_perDrawSceneSets{};
    std::array<uint32_t, kMaxFramesInFlight> m_perDrawSceneSetCursor{};
    std::array<size_t, kMaxFramesInFlight> m_bonesWriteCursorBytes{};
    uint32_t m_lastTransientFrameNumber = 0xffffffffu;
};

} // namespace render
} // namespace eyelib
