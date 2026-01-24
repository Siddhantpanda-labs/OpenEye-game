#pragma once

#include <cstdint>
#include <memory>
#include "eyelib/math/Types.h"

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

    bool Initialize(uint32_t width, uint32_t height);
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
    unsigned int ApplyPostProcessing(unsigned int hdrInputTexture, const GBuffer& gbuffer, const Settings& settings,
                                      const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                      const math::Mat4& invViewMatrix, const math::Mat4& invProjectionMatrix,
                                      const math::Vec3& viewPos, unsigned int skyboxTexture = 0, float time = 0.0f);

    /**
     * @brief Individual effect methods (for testing/debugging)
     */
    unsigned int ApplyToneMapping(unsigned int hdrTexture, const Settings& settings);
    unsigned int ExtractBloom(unsigned int hdrTexture, float threshold);
    unsigned int BlurTexture(unsigned int texture, int iterations);
    unsigned int ApplyGeneralBlur(unsigned int texture, float intensity, int iterations);
    unsigned int ApplyChromaticAberration(unsigned int texture, float intensity);
    unsigned int ApplyFXAA(unsigned int texture);
    unsigned int ApplySSAO(const GBuffer& gbuffer, const Settings& settings);
    unsigned int ApplySSR(unsigned int sceneTexture, const GBuffer& gbuffer, unsigned int skyboxTexture, const Settings& settings,
                          const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                          const math::Mat4& invViewMatrix, const math::Mat4& invProjectionMatrix,
                          const math::Vec3& viewPos);
    unsigned int ApplyMotionBlur(unsigned int texture, unsigned int velocityTexture, const Settings& settings);
    unsigned int ApplyVignette(unsigned int texture, const Settings& settings);
    unsigned int ApplyFilmGrain(unsigned int texture, const Settings& settings, float time = 0.0f);

    /**
     * @brief Get output texture (after full chain)
     */
    unsigned int GetOutputTexture() const { return m_outputTexture; }

private:
    void CreateFramebuffers();
    void DestroyFramebuffers();
    void LoadShaders();

    // Ping-pong FBOs for multi-pass effects
    unsigned int m_pingPongFBO[2] = {0, 0};
    unsigned int m_pingPongTexture[2] = {0, 0};

    // Output FBO (final result)
    unsigned int m_outputFBO = 0;
    unsigned int m_outputTexture = 0;

    // Bloom extraction FBO
    unsigned int m_bloomExtractFBO = 0;
    unsigned int m_bloomExtractTexture = 0;

    // SSAO FBO
    unsigned int m_ssaoFBO = 0;
    unsigned int m_ssaoTexture = 0;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

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
