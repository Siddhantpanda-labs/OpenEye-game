#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include <cstdint>
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

/** Forward: direct draw. Deferred: GBuffer + lighting pass. ForwardPlus: tiled forward (many lights per tile). */
enum class PipelineMode { Forward, Deferred, ForwardPlus };

/**
 * Orchestrates render passes. AddPass/SetOrder for extensibility;
 * Render(scene) runs Shadow -> GBuffer -> Lighting -> Transparency (deferred).
 */
class RenderPipeline {
public:
    RenderPipeline();
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;

    bool Initialize(uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);
    void Shutdown();

    void SetMode(PipelineMode mode) { m_mode = mode; }
    PipelineMode GetMode() const { return m_mode; }

    void Render(const RenderScene& scene);

    GBuffer* GetGBuffer();
    ShadowMap* GetShadowMap();
    PostProcessor* GetPostProcessor();
    FrameGraph* GetFrameGraph() { return m_frameGraph.get(); }
    const std::vector<FrameGraph::PassInfo>& GetFrameGraphPasses() const;

    void SetFrameSize(unsigned w, unsigned h);
    
    // Get HDR color texture (for post-processing input)
    unsigned int GetHDRColorTexture() const { return m_hdrColor; }

private:
    void EnsureResources(uint32_t width, uint32_t height);
    void CreateHDRFramebuffer();
    void DestroyHDRFramebuffer();
    void BuildAndExecuteFrameGraph(const RenderScene& scene);
    void RenderForward(const RenderScene& scene);
    void RenderForwardPlus(const RenderScene& scene);

    std::unique_ptr<GBuffer> m_gbuffer;
    std::unique_ptr<ShadowMap> m_shadowMap;
    std::unique_ptr<GBufferPass> m_gbufferPass;
    std::unique_ptr<ShadowPass> m_shadowPass;
    std::unique_ptr<LightingPass> m_lightingPass;
    std::unique_ptr<FrameGraph> m_frameGraph;
    std::unique_ptr<ClusteredLightGrid> m_clusteredLightGrid;
    std::unique_ptr<PostProcessor> m_postProcessor;
    std::shared_ptr<Shader> m_forwardPlusShader;

    unsigned int m_hdrFBO = 0;
    unsigned int m_hdrColor = 0;
    unsigned int m_hdrDepth = 0;

    // Forward+ SSBOs
    unsigned int m_pointLightSSBO = 0;
    unsigned int m_spotLightSSBO = 0;
    unsigned int m_lightIndexSSBO = 0;

    uint32_t m_width = 0, m_height = 0;
    PipelineMode m_mode = PipelineMode::Deferred;
    
    // Previous frame matrices for velocity calculation
    math::Mat4 m_prevView{1.0f};
    math::Mat4 m_prevProjection{1.0f};
};

} // namespace render
} // namespace eyelib
