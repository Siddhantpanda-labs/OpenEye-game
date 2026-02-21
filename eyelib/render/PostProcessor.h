#pragma once

#include <cstdint>
#include <memory>
#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHITypes.h"

namespace eyelib {
namespace render {

class GBuffer;
class Shader;
class FullscreenQuad;

/**
 * @brief Post-processing pipeline manager
 * 
 * Handles: HDR tone mapping, Bloom, FXAA, SSAO, SSR, Motion Blur, Vignette, Film Grain, etc.
 */
class PostProcessor {
public:
    PostProcessor();
    ~PostProcessor();

    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;

    bool Initialize(uint32_t width, uint32_t height, rhi::IDevice* device = nullptr);
    void Resize(uint32_t width, uint32_t height);
    void Shutdown();

    /**
     * @brief Post-processing settings
     */
    struct Settings {
        // Tone mapping
        float exposure = 1.0f;
        bool useACES = true;  // ACES filmic tone mapping

        // Bloom
        bool enableBloom = true;
        float bloomThreshold = 1.0f;
        float bloomIntensity = 0.5f;
        int bloomIterations = 5;

        // FXAA
        bool enableFXAA = true;

        // General Blur
        bool enableBlur = false;
        float blurIntensity = 1.0f;
        int blurIterations = 3;

        // Chromatic Aberration
        bool enableChromaticAberration = false;
        float chromaticAberrationIntensity = 0.002f;

        // SSAO
        bool enableSSAO = false;
        float ssaoRadius = 0.5f;
        float ssaoBias = 0.025f;
        int ssaoSamples = 16;

        // SSR (Screen-Space Reflections)
        bool enableSSR = false;
        float ssrStepSize = 0.1f;
        int ssrMaxSteps = 32;
        float ssrThickness = 0.01f;

        // Motion Blur
        bool enableMotionBlur = false;
        float motionBlurIntensity = 1.0f;
        int motionBlurSamples = 8;

        // Vignette
        bool enableVignette = false;
        float vignetteIntensity = 0.5f;
        float vignetteRadius = 0.75f;
        float vignetteSoftness = 0.5f;

        // Film Grain
        bool enableFilmGrain = false;
        float filmGrainIntensity = 0.1f;
    };

    /**
     * @brief Apply full post-processing chain
     * @param hdrInputTexture HDR color texture from lighting pass
     * @param gbuffer GBuffer for SSAO/SSR
     * @param settings Post-processing settings
     * @param viewMatrix, projectionMatrix, invViewMatrix, invProjectionMatrix, viewPos For SSR
     * @param skyboxTexture Skybox cubemap for SSR fallback (0 if not available)
     * @return Final LDR output texture
     */
    rhi::ITexture* ApplyPostProcessing(rhi::ITexture* hdrInputTexture, const GBuffer& gbuffer, const Settings& settings,
                                       const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                       const math::Mat4& invViewMatrix, const math::Mat4& invProjectionMatrix,
                                       const math::Vec3& viewPos, rhi::ITexture* skyboxTexture = nullptr, float time = 0.0f);

    /**
     * @brief Individual effect methods (for testing/debugging)
     */
    rhi::ITexture* ApplyToneMapping(rhi::ITexture* hdrTexture, const Settings& settings);
    rhi::ITexture* ExtractBloom(rhi::ITexture* hdrTexture, float threshold);
    rhi::ITexture* BlurTexture(rhi::ITexture* texture, int iterations);
    rhi::ITexture* ApplyGeneralBlur(rhi::ITexture* texture, float intensity, int iterations);
    rhi::ITexture* ApplyChromaticAberration(rhi::ITexture* texture, float intensity);
    rhi::ITexture* ApplyFXAA(rhi::ITexture* texture);
    rhi::ITexture* ApplySSAO(const GBuffer& gbuffer, const Settings& settings);
    rhi::ITexture* ApplySSR(rhi::ITexture* sceneTexture, const GBuffer& gbuffer, rhi::ITexture* skyboxTexture, const Settings& settings,
                            const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                            const math::Mat4& invViewMatrix, const math::Mat4& invProjectionMatrix,
                            const math::Vec3& viewPos);
    rhi::ITexture* ApplyMotionBlur(rhi::ITexture* texture, rhi::ITexture* velocityTexture, const Settings& settings);
    rhi::ITexture* ApplyVignette(rhi::ITexture* texture, const Settings& settings);
    rhi::ITexture* ApplyFilmGrain(rhi::ITexture* texture, const Settings& settings, float time = 0.0f);

    /**
     * @brief Get output texture (after full chain)
     */
    rhi::IRenderTarget* GetOutputRenderTargetRHI() const { return m_outputRT; }
    rhi::ITexture* GetOutputTextureRHI() const;
    /** E6: Ping-pong RT for graph-driven post (e.g. tone-mapped intermediate). Index 0 or 1. */
    rhi::IRenderTarget* GetPingPongRT(int index) const { return (index >= 0 && index <= 1) ? m_pingPongRT[index] : nullptr; }
    bool HasValidOutputFrame() const { return m_lastOutputValid; }

    /**
     * E6: Add post-processing as frame-graph passes. Each effect is one pass; order is modular.
     * hdrInput: FG handle for HDR color. toneMappedOutput: FG handle for tone-map result (e.g. ping-pong RT texture). finalOutput: FG handle for final LDR (e.g. m_outputRT texture).
     * Call after adding main passes; adds ToneMap then FXAA. Side-effect.
     */
    void AddPostPassesToFrameGraph(class FrameGraph* fg, int hdrInput, int toneMappedOutput, int finalOutput,
                                  const Settings& settings, rhi::ICommandList* cmd);

    /** Reload all shaders that were loaded by name (for hot-reload when files change). */
    void ReloadShaders();

private:
    void CreateFramebuffers();
    void DestroyFramebuffers();
    rhi::ITexture* GetColorTexture(rhi::IRenderTarget* rt) const;
    void LoadShaders();

    rhi::IDevice* m_deviceRHI = nullptr;

    // Descriptor set for single-texture passes (Phase 3: layout-driven binding)
    rhi::IPipelineLayout* m_singleTexLayout = nullptr;
    rhi::IDescriptorSet* m_singleTexSet = nullptr;

    // Ping-pong render targets for multi-pass effects
    rhi::IRenderTarget* m_pingPongRT[2] = {nullptr, nullptr};

    // Output render target (final result)
    rhi::IRenderTarget* m_outputRT = nullptr;

    // Bloom extraction render target
    rhi::IRenderTarget* m_bloomExtractRT = nullptr;

    // SSAO render target
    rhi::IRenderTarget* m_ssaoRT = nullptr;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_lastOutputValid = false;

    // Shaders
    std::shared_ptr<Shader> m_toneMappingShader;
    std::shared_ptr<Shader> m_bloomExtractShader;
    std::shared_ptr<Shader> m_bloomBlurShader;
    std::shared_ptr<Shader> m_bloomCompositeShader;
    std::shared_ptr<Shader> m_blurShader;
    std::shared_ptr<Shader> m_chromaticAberrationShader;
    std::shared_ptr<Shader> m_fxaaShader;
    std::shared_ptr<Shader> m_ssaoShader;
    std::shared_ptr<Shader> m_ssrShader;
    std::shared_ptr<Shader> m_motionBlurShader;
    std::shared_ptr<Shader> m_vignetteShader;
    std::shared_ptr<Shader> m_filmGrainShader;
    std::shared_ptr<Shader> m_ssaoBlendShader;  // For blending SSAO with scene
};

} // namespace render
} // namespace eyelib
