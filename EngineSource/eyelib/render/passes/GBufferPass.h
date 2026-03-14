#pragma once

#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/RenderContext.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <functional>
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

class Mesh;
class MaterialInstance;
class Shader;
class ShaderCache;

/** Renders geometry to G-buffer (position, normal, albedo, PBR). */
class GBufferPass {
public:
    GBufferPass();
    ~GBufferPass() = default;

    /** E9: When set and device supports bindless, pass uses bindless texture handles instead of binding units. */
    void SetBindlessHeap(rhi::IBindlessHeap* heap) { m_bindlessHeap = heap; }
    void SetBindlessGetHandle(std::function<rhi::BindlessHandle(rhi::ITexture*)> fn) { m_bindlessGetHandle = std::move(fn); }
    /** @deprecated Geometry shaders come from ShaderCache only; no-op. */
    void SetGeometryShader(std::shared_ptr<Shader> shader);
    /** @deprecated Returns nullptr; shaders are obtained via ShaderCache per draw. */
    std::shared_ptr<Shader> GetGeometryShader() const;
    /** Required: pass uses ShaderCache + MaterialCompiler for all GBuffer shaders (stage × instance type × material). */
    void SetShaderCache(ShaderCache* cache) { m_shaderCache = cache; }
    ShaderCache* GetShaderCache() const { return m_shaderCache; }

    /** Render drawables into the G-buffer (context-based). */
    void Render(GBuffer& gbuffer,
                const RenderContext& ctx,
                const std::vector<Drawable>& drawables,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Mat4& prevView,
                const math::Mat4& prevProjection);

    /** Render drawables into the G-buffer. Device and cmd must be non-null. */
    void Render(GBuffer& gbuffer,
                rhi::IDevice* device,
                rhi::ICommandList* cmd,
                const std::vector<Drawable>& drawables,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Mat4& prevView,
                const math::Mat4& prevProjection);

    /** Zero-copy: feed invokes consumer for each drawable (context-based). */
    using DrawableFeed = std::function<void(std::function<void(const Drawable&)>)>;
    void Render(GBuffer& gbuffer,
                const RenderContext& ctx,
                DrawableFeed feedDrawables,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Mat4& prevView,
                const math::Mat4& prevProjection);

    /** Zero-copy: feed invokes consumer for each drawable. Device and cmd must be non-null. */
    void Render(GBuffer& gbuffer,
                rhi::IDevice* device,
                rhi::ICommandList* cmd,
                DrawableFeed feedDrawables,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Mat4& prevView,
                const math::Mat4& prevProjection);

    /**
     * GPU-driven path (E8): draw using an indirect buffer from GpuCullPass (context-based).
     */
    void RenderIndirect(GBuffer& gbuffer,
                        const RenderContext& ctx,
                        rhi::IBuffer* indirectBuffer,
                        uint32_t drawCount,
                        uint32_t stride,
                        const Mesh* mesh,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Mat4* prevView = nullptr,
                        const math::Mat4* prevProjection = nullptr);

    /**
     * GPU-driven path (E8): draw using an indirect buffer from GpuCullPass.
     * When m_device supports MultiDrawIndirect and indirectBuffer/drawCount/mesh are valid,
     * binds pipeline, shared uniforms (view/proj), mesh VB/IB, and calls MultiDrawIndexedIndirect.
     * No-op otherwise (capability-driven fallback).
     */
    void RenderIndirect(GBuffer& gbuffer,
                        rhi::IDevice* device,
                        rhi::ICommandList* cmd,
                        rhi::IBuffer* indirectBuffer,
                        uint32_t drawCount,
                        uint32_t stride,
                        const Mesh* mesh,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Mat4* prevView = nullptr,
                        const math::Mat4* prevProjection = nullptr);

    /**
     * GPU-driven scene path: same as RenderIndirect but with per-draw model matrices (context-based).
     */
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
                                       const math::Mat4* prevProjection = nullptr);

    /**
     * GPU-driven scene path: same as RenderIndirect but with per-draw model matrices in a buffer.
     * Vertex shader uses gl_DrawID to fetch u_ModelArray[gl_DrawID]. Use when all drawables share one mesh.
     */
    void RenderIndirectWithPerDrawData(GBuffer& gbuffer,
                                       rhi::IDevice* device,
                                       rhi::ICommandList* cmd,
                                       rhi::IBuffer* indirectBuffer,
                                       uint32_t drawCount,
                                       uint32_t stride,
                                       const Mesh* mesh,
                                       rhi::IBuffer* perDrawTransformBuffer,
                                       uint32_t transformCount,
                                       const math::Mat4& view,
                                       const math::Mat4& projection,
                                       const math::Mat4* prevView = nullptr,
                                       const math::Mat4* prevProjection = nullptr);

    GBuffer* GetGBuffer() { return m_currentGBuffer; }

    /**
     * Instanced draw (7.3): one mesh + material, N instances. instanceMatrixBuffer must contain instanceCount mat4s (column-major).
     * Requires SetShaderCache; uses ShaderKey(GBuffer, Instanced, material).
     */
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
                         int32_t submeshIndex = -1);

    /** Hot-reload: clears ShaderCache so next getOrCompile recompiles from templates. */
    void ReloadShaders();

private:
    void RenderDrawables(GBuffer& gbuffer,
                         rhi::ICommandList* cmd,
                         rhi::IDevice* device,
                         DrawableFeed feedDrawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection);

    GBuffer* m_currentGBuffer = nullptr;
    rhi::IBindlessHeap* m_bindlessHeap = nullptr;
    std::function<rhi::BindlessHandle(rhi::ITexture*)> m_bindlessGetHandle;
    ShaderCache* m_shaderCache = nullptr;
};

} // namespace render
} // namespace eyelib
