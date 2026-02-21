#pragma once

#include "eyelib/render/IRenderSceneView.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/passes/GpuCullPass.h"
#include "eyelib/render/passes/LightCullPass.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace render {

class GBuffer;
class ShadowMap;
class GBufferPass;
class ShadowPass;
class LightingPass;
class ClusteredLightGrid;
class PostProcessor;
class GpuProfilerRHI;
class Mesh;
class Shader;
class TextureCube;
class FullscreenQuad;

/** Forward: frame-graph (GPU Cull, Shadow, Forward Opaque, Transparency, Skybox, Post). Deferred: frame-graph (GPU Cull, Shadow, GBuffer, Lighting, Transparency, Skybox, Post). ForwardPlus: tiled forward (many lights), skybox drawn after; legacy path. */
enum class PipelineMode { Forward, Deferred, ForwardPlus };

/**
 * Orchestrates render passes via a frame graph for Forward and Deferred.
 * Render(view) dispatches by mode: Deferred -> BuildAndExecuteFrameGraph; Forward -> BuildAndExecuteForwardFrameGraph; ForwardPlus -> RenderForwardPlus (legacy).
 */
class RenderPipeline {
public:
    using PrePostCallback = std::function<void(rhi::IRenderTarget* hdrRT, unsigned int width, unsigned int height, const RenderScene& scene)>;
    RenderPipeline();
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;

    /** Initialize with injected RHI device (no backend include in this module). */
    bool Initialize(std::unique_ptr<rhi::IDevice> device, uint32_t width, uint32_t height);
    /** Resize pipeline (GBuffer, HDR RT, post-process). May only be called at frame boundary, before Render() for this frame. Never call mid-frame or while a command list is active. */
    void Resize(uint32_t width, uint32_t height);
    void Shutdown();

    void SetMode(PipelineMode mode) { m_mode = mode; }
    PipelineMode GetMode() const { return m_mode; }

    /** Render scene (ECS or converted from API at the api layer). */
    void Render(const RenderScene& scene);
    /** Render from a read-only view (zero-copy when view wraps API scene). */
    void Render(const IRenderSceneView& view);
    void BlitToScreen();
    /** Wait for GPU to finish all submitted work (e.g. before swap to avoid flicker). */
    void Finish();
    /** Returns true when the pipeline has a valid render target to present. */
    bool HasValidOutput() const;

    rhi::IDevice* GetRHIDevice() { return m_device.get(); }
    rhi::ICommandList* GetRHICommandList() { return m_device ? m_device->GetImmediateCommandList() : nullptr; }

    GBuffer* GetGBuffer();
    ShadowMap* GetShadowMap();
    PostProcessor* GetPostProcessor();
    FrameGraph* GetFrameGraph() { return m_frameGraph.get(); }
    const std::vector<FrameGraph::PassInfo>& GetFrameGraphPasses() const;

    void SetFrameSize(unsigned w, unsigned h);
    void SetPrePostProcessCallback(PrePostCallback cb) { m_prePostCallback = std::move(cb); }

    /** When true and GpuCullPass is supported, deferred GBuffer uses MultiDrawIndexedIndirect (E8). Default false. */
    void SetUseGpuDrivenGBuffer(bool use) { m_useGpuDrivenGBuffer = use; }
    bool GetUseGpuDrivenGBuffer() const { return m_useGpuDrivenGBuffer; }

    /** E9: Bindless heap (null when !supportsBindless). */
    rhi::IBindlessHeap* GetBindlessHeap() const { return m_bindlessHeap; }
    /** E9: Get or allocate bindless handle for texture; returns kInvalidBindlessHandle when bindless unsupported or texture null. */
    rhi::BindlessHandle GetOrAllocateBindlessHandle(rhi::ITexture* texture);

    /** HDR color texture for post-processing (RHI path only; no GL id in this module). */
    rhi::ITexture* GetHDRColorTextureRHI() const;

    /** Render target that will be blitted to screen this frame (post output when valid, else HDR). */
    rhi::IRenderTarget* GetCurrentFrameOutputRT() const;
    /** Color texture of the current frame output (for display; same as BlitToScreen source). */
    rhi::ITexture* GetCurrentFrameOutputColorTextureRHI() const;

private:
    void EnsureResources(uint32_t width, uint32_t height);
    void CreateHDRFramebuffer();
    void DestroyHDRFramebuffer();
    void BuildAndExecuteFrameGraph(const IRenderSceneView& view);
    /** Forward mode: frame graph with GPU Cull, Shadow, Forward Opaque, Transparency, Post. */
    void BuildAndExecuteForwardFrameGraph(const IRenderSceneView& view);
    void RenderForward(const IRenderSceneView& view);
    /** Opaque draw loop for forward (lighting + optional shadow). Used by Forward frame graph. */
    void RenderForwardOpaque(rhi::ICommandList* cmd, const IRenderSceneView& view);
    void RenderForwardPlus(const IRenderSceneView& view);
    /** Skybox as a frame-graph pass: writes to HDR so post reads it. Used by Deferred and Forward. */
    void AddSkyboxPassToFrameGraph(const IRenderSceneView& view, FGTextureHandle hdrColor, bool useGbufferDepth);
    void RenderSkybox(const IRenderSceneView& view, bool useGbufferDepth, rhi::IRenderTarget* targetOverride = nullptr);
    void RenderSkyboxImpl(const CameraView& camera, const std::shared_ptr<TextureCube>& skybox, bool useGbufferDepth, rhi::IRenderTarget* targetOverride = nullptr);
    void EnsureSkyboxShader();
    /** C1: Single opaque-geometry entry point for GBuffer. Chooses CPU submit (per-draw) or GPU submit (indirect) by capability + data. */
    void RenderOpaqueGeometryToGBuffer(rhi::ICommandList* cmd, const IRenderSceneView& view, const CameraView& cam, uint32_t gpuCullDrawListCount);

    /** Shared: build draw list from view, upload to GPU, configure GpuCullPass. Returns draw list count. Used by deferred and forward frame graphs. */
    uint32_t PrepareGpuCullFromView(const IRenderSceneView& view, const CameraView& cam);

    std::unique_ptr<rhi::IDevice> m_device;
    std::unique_ptr<GBuffer> m_gbuffer;
    std::unique_ptr<ShadowMap> m_shadowMap;
    std::unique_ptr<GBufferPass> m_gbufferPass;
    std::unique_ptr<ShadowPass> m_shadowPass;
    std::unique_ptr<LightingPass> m_lightingPass;
    std::unique_ptr<GpuCullPass> m_gpuCullPass;
    std::unique_ptr<LightCullPass> m_lightCullPass;
    std::unique_ptr<FrameGraph> m_frameGraph;
    std::unique_ptr<ClusteredLightGrid> m_clusteredLightGrid;
    std::unique_ptr<PostProcessor> m_postProcessor;
    std::unique_ptr<GpuProfilerRHI> m_gpuProfilerRHI;
    std::shared_ptr<Shader> m_forwardPlusShader;
    std::shared_ptr<Shader> m_forwardPlusSkinnedShader;
    std::shared_ptr<Shader> m_skyboxShader;

    /** Quad mesh for GPU-driven GBuffer path (matches current cull output: 6 indices). */
    std::unique_ptr<Mesh> m_gpuDrivenQuad;

    /** If true, deferred GBuffer uses RenderIndirect when GpuCullPass is supported (E8). */
    bool m_useGpuDrivenGBuffer = false;

    /** A2: Draw list for GpuCullPass (scene-fed); max 256 entries. */
    /** Per-draw transforms for GPU-driven path (same order as draw list). */
    rhi::IBuffer* m_gpuCullTransformBuffer = nullptr;
    /** Scratch for building draw list each frame (A2). */
    std::vector<GpuCullDrawEntry> m_scratchGpuCullDrawList;
    /** Scratch transforms in same order as draw list (for indirect scene path). */
    std::vector<math::Mat4> m_scratchGpuCullTransforms;
    /** True when all opaque drawables share the same mesh (enables indirect scene path). */
    bool m_gpuCullSingleMesh = false;
    /** Representative mesh when single-mesh; kept alive for GBuffer pass. */
    std::shared_ptr<Mesh> m_gpuCullRepresentativeMesh;

    /** Previous frame depth RT for GPU occlusion culling (same size as GBuffer). */
    rhi::IRenderTarget* m_prevFrameDepthRT = nullptr;
    /** True after first frame's depth copy so cull can use occlusion. */
    bool m_havePrevDepth = false;

    /** E9: Bindless heap (created when supportsBindless). */
    rhi::IBindlessHeap* m_bindlessHeap = nullptr;
    /** E9: Cache ITexture* -> BindlessHandle for GetOrAllocateBindlessHandle. */
    std::unordered_map<rhi::ITexture*, rhi::BindlessHandle> m_bindlessHandleCache;

    rhi::IRenderTarget* m_hdrRenderTarget = nullptr;

    // Forward+ SSBOs (RHI)
    rhi::IBuffer* m_pointLightBuffer = nullptr;
    rhi::IBuffer* m_spotLightBuffer = nullptr;
    rhi::IBuffer* m_lightIndexBuffer = nullptr;

    uint32_t m_width = 0, m_height = 0;       // render size (letterbox aspect)
    uint32_t m_windowWidth = 0, m_windowHeight = 0;  // output window size for letterbox blit
    PipelineMode m_mode = PipelineMode::Deferred;
    PrePostCallback m_prePostCallback;
    
    // Previous frame matrices for velocity calculation
    math::Mat4 m_prevView{1.0f};
    math::Mat4 m_prevProjection{1.0f};

    // Forward+ only: scratch for api lights -> internal layout (SSBO std140); drawables are zero-copy
    std::vector<PointLight> m_scratchPointLights;
    std::vector<SpotLight> m_scratchSpotLights;
    // Scratch for transparent drawables when using view (sorting requires a list)
    mutable std::vector<Drawable> m_scratchTransparent;

#ifndef NDEBUG
    /** Debug guard: set during Render() to catch invalid Resize() mid-frame. */
    bool m_rendering = false;
#endif
};

} // namespace render
} // namespace eyelib
