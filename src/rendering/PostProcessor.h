#pragma once

#include <glad/glad.h>
#include <memory>
#include <glm/glm.hpp>

namespace OpenEye {
namespace Rendering {

class Shader;
class GBuffer;

// Post-processing pipeline manager
// Handles: HDR tone mapping, Bloom, FXAA, SSAO
class PostProcessor {
public:
    PostProcessor();
    ~PostProcessor();

    // Non-copyable
    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;

    bool Initialize(uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);

    // Post-processing settings
    struct Settings {
        // Tone mapping
        float exposure = 1.0f;
        bool useACES = true; // ACES filmic tone mapping
        
        // Bloom
        bool enableBloom = true;
        float bloomThreshold = 1.0f; // Extract pixels brighter than this
        float bloomIntensity = 0.5f;
        int bloomIterations = 5; // Number of blur passes
        
        // FXAA
        bool enableFXAA = true;
        
        // General Blur
        bool enableBlur = false;
        float blurIntensity = 1.0f; // Blur strength (multiplier for blur radius)
        int blurIterations = 3; // Number of blur passes
        
        // Chromatic Aberration (RGB channel separation)
        bool enableChromaticAberration = false;
        float chromaticAberrationIntensity = 0.002f; // Offset distance (in screen space, range: 0.0-0.05)
        
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

    // Apply post-processing chain
    // Input: HDR color texture (from lighting pass)
    // Output: Final LDR color to screen or texture
    GLuint ApplyPostProcessing(GLuint hdrInputTexture, const GBuffer& gbuffer, const Settings& settings);

    // Individual effect methods (for testing/debugging)
    GLuint ApplyToneMapping(GLuint hdrTexture, const Settings& settings);
    GLuint ExtractBloom(GLuint hdrTexture, float threshold);
    GLuint BlurTexture(GLuint texture, int iterations);
    GLuint ApplyGeneralBlur(GLuint texture, float intensity, int iterations);
    GLuint ApplyChromaticAberration(GLuint texture, float intensity);
    GLuint ApplyFXAA(GLuint texture);
    GLuint ApplySSAO(const GBuffer& gbuffer, const Settings& settings);
    GLuint ApplySSR(GLuint sceneTexture, const GBuffer& gbuffer, GLuint skyboxTexture, const Settings& settings);
    GLuint ApplyMotionBlur(GLuint texture, const GBuffer& gbuffer, const Settings& settings);
    GLuint ApplyVignette(GLuint texture, const Settings& settings);
    GLuint ApplyFilmGrain(GLuint texture, const Settings& settings);

    // Get output texture (after full chain)
    GLuint GetOutputTexture() const { return m_outputTexture; }

private:
    void CreateFramebuffers();
    void DestroyFramebuffers();
    
    // Ping-pong FBOs for multi-pass effects
    GLuint m_pingPongFBO[2] = {0, 0};
    GLuint m_pingPongTexture[2] = {0, 0};
    
    // Output FBO (final result)
    GLuint m_outputFBO = 0;
    GLuint m_outputTexture = 0;
    
    // Bloom extraction FBO
    GLuint m_bloomExtractFBO = 0;
    GLuint m_bloomExtractTexture = 0;
    
    // SSAO FBO
    GLuint m_ssaoFBO = 0;
    GLuint m_ssaoTexture = 0;
    
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
    
    void LoadShaders();
};

} // namespace Rendering
} // namespace OpenEye

