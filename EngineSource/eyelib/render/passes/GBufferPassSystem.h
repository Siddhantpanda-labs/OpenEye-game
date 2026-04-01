#pragma once

#include "eyelib/render/RenderContext.h"
#include "eyelib/render/passes/GBufferPassOpaqueParams.h"
#include <functional>
#include <vector>

namespace eyelib {
namespace render {

class GBuffer;
struct Drawable;
class Material;
class MaterialInstance;
class Mesh;
class ShaderCache;

class GBufferPassSystem {
public:
    GBufferPassSystem() = default;
    ~GBufferPassSystem();

    /** Releases the small SceneBuffers fallback SSBO; call before device teardown or pass Reset (optional device matches allocator). */
    void Shutdown(rhi::IDevice* device = nullptr);

    void SetShaderCache(ShaderCache* cache);
    void SetBindlessHeap(rhi::IBindlessHeap* heap);
    void SetBindlessGetHandle(std::function<rhi::BindlessHandle(rhi::ITexture*)> fn);
    void SetResourceSpaceSets(rhi::IDescriptorSet* frameSet, rhi::IDescriptorSet* materialSet, rhi::IDescriptorSet* sceneBuffersSet);
    void SetBonesBuffer(rhi::IBuffer* buffer);
    void ReloadShaders();

    void RenderDrawablesDirect(GBuffer& gbuffer,
                               const RenderContext& ctx,
                               const std::vector<Drawable>& drawables,
                               const math::Mat4& view,
                               const math::Mat4& projection,
                               const math::Mat4& prevView,
                               const math::Mat4& prevProjection) const;

    void RenderOpaqueGeometry(const GBufferPassOpaqueParams& p) const;

private:
    /** SceneBuffers layout (set 3) declares four SSBO bindings; Vulkan requires every binding to be written before bind. */
    void EnsureSceneBuffersDescriptorWrites(rhi::IDevice* device, rhi::IDescriptorSet* sceneSet) const;
    rhi::IDescriptorSet* AcquireMaterialSetForDraw() const;
    rhi::IDescriptorSet* AcquireSceneSetForDraw() const;

    using DrawableFeed = std::function<void(std::function<void(const Drawable&)>)>;

    void RenderDrawables(GBuffer& gbuffer,
                         rhi::ICommandList* cmd,
                         rhi::IDevice* device,
                         DrawableFeed feedDrawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection) const;

    void RenderIndirect(GBuffer& gbuffer,
                        const RenderContext& ctx,
                        rhi::IBuffer* indirectBuffer,
                        uint32_t drawCount,
                        uint32_t stride,
                        const Mesh* mesh,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Mat4* prevView = nullptr,
                        const math::Mat4* prevProjection = nullptr) const;

    void RenderIndirectWithPerDrawData(GBuffer& gbuffer,
                                       const RenderContext& ctx,
                                       rhi::IBuffer* indirectBuffer,
                                       uint32_t drawCount,
                                       uint32_t stride,
                                       const Mesh* mesh,
                                       rhi::IBuffer* perDrawTransformBuffer,
                                       uint32_t transformCount,
                                       const math::Mat4& view,
                                       const math::Mat4& projection,
                                       const math::Mat4* prevView = nullptr,
                                       const math::Mat4* prevProjection = nullptr) const;

    void RenderInstanced(GBuffer& gbuffer,
                         rhi::IDevice* device,
                         rhi::ICommandList* cmd,
                         const Mesh* mesh,
                         const Material* material,
                         const MaterialInstance* materialInstance,
                         rhi::IBuffer* instanceMatrixBuffer,
                         uint32_t instanceCount,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection,
                         int32_t submeshIndex = -1) const;

    rhi::IBindlessHeap* m_bindlessHeap = nullptr;
    std::function<rhi::BindlessHandle(rhi::ITexture*)> m_bindlessGetHandle;
    ShaderCache* m_shaderCache = nullptr;
    rhi::IDescriptorSet* m_gbufferFrameSet = nullptr;
    rhi::IDescriptorSet* m_gbufferMaterialSet = nullptr;
    rhi::IDescriptorSet* m_gbufferSceneBuffersSet = nullptr;
    rhi::IBuffer* m_gbufferBonesBuffer = nullptr;
    /** Dummy storage buffer so bindings 0–3 are valid for static meshes (same pattern as ShadowPass / LightCullPass). */
    mutable rhi::IBuffer* m_gbufferSceneFallbackBuffer = nullptr;
    mutable rhi::IDevice* m_sceneFallbackDevice = nullptr;
    mutable std::function<rhi::IDescriptorSet*()> m_acquireMaterialDrawSet;
    mutable std::function<rhi::IDescriptorSet*()> m_acquireSceneDrawSet;
};

}  // namespace render
}  // namespace eyelib
