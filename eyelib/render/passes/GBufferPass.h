#pragma once

#include "eyelib/render/GBuffer.h"
#include "eyelib/render/RenderContext.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <functional>
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

class Mesh;
class Shader;

/** Renders geometry to G-buffer (position, normal, albedo, PBR). */
class GBufferPass {
public:
    GBufferPass();
    ~GBufferPass() = default;

    /** E9: When set and device supports bindless, pass uses bindless texture handles instead of binding units. */
    void SetBindlessHeap(rhi::IBindlessHeap* heap) { m_bindlessHeap = heap; }
    void SetBindlessGetHandle(std::function<rhi::BindlessHandle(rhi::ITexture*)> fn) { m_bindlessGetHandle = std::move(fn); }
    void SetGeometryShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetGeometryShader() const { return m_geometryShader; }

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

    /** Reload geometry shader (for hot-reload when shader files change). */
    void ReloadShaders();

private:
    void EnsureDefaultShader(rhi::IDevice* device);
    void RenderDrawables(GBuffer& gbuffer,
                         rhi::ICommandList* cmd,
                         rhi::IDevice* device,
                         DrawableFeed feedDrawables,
                         const math::Mat4& view,
                         const math::Mat4& projection,
                         const math::Mat4& prevView,
                         const math::Mat4& prevProjection);

    std::shared_ptr<Shader> m_geometryShader;
    GBuffer* m_currentGBuffer = nullptr;
    rhi::IBindlessHeap* m_bindlessHeap = nullptr;
    std::function<rhi::BindlessHandle(rhi::ITexture*)> m_bindlessGetHandle;
};

} // namespace render
} // namespace eyelib
