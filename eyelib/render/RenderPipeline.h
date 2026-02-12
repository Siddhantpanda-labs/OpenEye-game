#pragma once

#include "eyelib/render/IRenderSceneView.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <functional>
#include <memory>
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
class Shader;
class TextureCube;
class FullscreenQuad;

/** Forward: direct draw. Deferred: GBuffer + lighting pass. ForwardPlus: tiled forward (many lights per tile). */
enum class PipelineMode { Forward, Deferred, ForwardPlus };

/**
 * Orchestrates render passes. AddPass/SetOrder for extensibility;
 * Render(scene) runs Shadow -> GBuffer -> Lighting -> Transparency (deferred).
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
    
    /** HDR color texture for post-processing (RHI path only; no GL id in this module). */
    rhi::ITexture* GetHDRColorTextureRHI() const;

private:
    void EnsureResources(uint32_t width, uint32_t height);
    void CreateHDRFramebuffer();
    void DestroyHDRFramebuffer();
    void BuildAndExecuteFrameGraph(const IRenderSceneView& view);
    void RenderForward(const IRenderSceneView& view);
    void RenderForwardPlus(const IRenderSceneView& view);
    void RenderSkybox(const IRenderSceneView& view, bool useGbufferDepth);
    void RenderSkyboxImpl(const CameraView& camera, const std::shared_ptr<TextureCube>& skybox, bool useGbufferDepth);
    void EnsureSkyboxShader();

    std::unique_ptr<rhi::IDevice> m_device;
    std::unique_ptr<GBuffer> m_gbuffer;
    std::unique_ptr<ShadowMap> m_shadowMap;
    std::unique_ptr<GBufferPass> m_gbufferPass;
    std::unique_ptr<ShadowPass> m_shadowPass;
    std::unique_ptr<LightingPass> m_lightingPass;
    std::unique_ptr<FrameGraph> m_frameGraph;
    std::unique_ptr<ClusteredLightGrid> m_clusteredLightGrid;
    std::unique_ptr<PostProcessor> m_postProcessor;
    std::shared_ptr<Shader> m_forwardPlusShader;
    std::shared_ptr<Shader> m_forwardPlusSkinnedShader;
    std::shared_ptr<Shader> m_skyboxShader;

    rhi::IRenderTarget* m_hdrRenderTarget = nullptr;

    // Forward+ SSBOs (RHI)
    rhi::IBuffer* m_pointLightBuffer = nullptr;
    rhi::IBuffer* m_spotLightBuffer = nullptr;
    rhi::IBuffer* m_lightIndexBuffer = nullptr;

    uint32_t m_width = 0, m_height = 0;
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
