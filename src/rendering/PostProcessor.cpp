#include "PostProcessor.h"
#include "resources/Shader.h"
#include "GBuffer.h"
#include "FullscreenQuad.h"
#include "core/log/Log.h"
#include <glm/glm.hpp>

namespace OpenEye {
namespace Rendering {

PostProcessor::PostProcessor() = default;

PostProcessor::~PostProcessor() {
    DestroyFramebuffers();
}

void PostProcessor::DestroyFramebuffers() {
    if (m_pingPongFBO[0]) glDeleteFramebuffers(1, &m_pingPongFBO[0]);
    if (m_pingPongFBO[1]) glDeleteFramebuffers(1, &m_pingPongFBO[1]);
    if (m_pingPongTexture[0]) glDeleteTextures(1, &m_pingPongTexture[0]);
    if (m_pingPongTexture[1]) glDeleteTextures(1, &m_pingPongTexture[1]);
    if (m_outputFBO) glDeleteFramebuffers(1, &m_outputFBO);
    if (m_outputTexture) glDeleteTextures(1, &m_outputTexture);
    if (m_bloomExtractFBO) glDeleteFramebuffers(1, &m_bloomExtractFBO);
    if (m_bloomExtractTexture) glDeleteTextures(1, &m_bloomExtractTexture);
    if (m_ssaoFBO) glDeleteFramebuffers(1, &m_ssaoFBO);
    if (m_ssaoTexture) glDeleteTextures(1, &m_ssaoTexture);
}

bool PostProcessor::Initialize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    
    LoadShaders();
    CreateFramebuffers();
    
    return m_outputTexture != 0;
}

void PostProcessor::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    m_width = width;
    m_height = height;
    
    DestroyFramebuffers();
    CreateFramebuffers();
}

void PostProcessor::CreateFramebuffers() {
    // Ping-pong FBOs (for blur passes)
    glGenFramebuffers(2, m_pingPongFBO);
    glGenTextures(2, m_pingPongTexture);
    
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_pingPongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, m_pingPongTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_width, m_height, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pingPongTexture[i], 0);
        
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            OE_ERROR_F("PostProcessor: Ping-pong FBO %d incomplete: 0x%x", i, status);
        }
    }
    
    // Bloom extract FBO
    glGenFramebuffers(1, &m_bloomExtractFBO);
    glGenTextures(1, &m_bloomExtractTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomExtractFBO);
    glBindTexture(GL_TEXTURE_2D, m_bloomExtractTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_width, m_height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomExtractTexture, 0);
    
    // Output FBO (LDR final result)
    glGenFramebuffers(1, &m_outputFBO);
    glGenTextures(1, &m_outputTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glBindTexture(GL_TEXTURE_2D, m_outputTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_outputTexture, 0);
    
    // SSAO FBO
    glGenFramebuffers(1, &m_ssaoFBO);
    glGenTextures(1, &m_ssaoTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO);
    glBindTexture(GL_TEXTURE_2D, m_ssaoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoTexture, 0);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::LoadShaders() {
    m_toneMappingShader = std::make_shared<Shader>();
    if (!m_toneMappingShader->LoadFromFile("assets/shaders/passes/post/tonemap.vert", "assets/shaders/passes/post/tonemap.frag")) {
        OE_WARN("PostProcessor: Failed to load tone mapping shader");
    }
    
    m_bloomExtractShader = std::make_shared<Shader>();
    if (!m_bloomExtractShader->LoadFromFile("assets/shaders/passes/post/bloom_extract.vert", "assets/shaders/passes/post/bloom_extract.frag")) {
        OE_WARN("PostProcessor: Failed to load bloom extract shader");
    }
    
    m_bloomBlurShader = std::make_shared<Shader>();
    if (!m_bloomBlurShader->LoadFromFile("assets/shaders/passes/post/bloom_blur.vert", "assets/shaders/passes/post/bloom_blur.frag")) {
        OE_WARN("PostProcessor: Failed to load bloom blur shader");
    }
    
    m_bloomCompositeShader = std::make_shared<Shader>();
    if (!m_bloomCompositeShader->LoadFromFile("assets/shaders/passes/post/bloom_composite.vert", "assets/shaders/passes/post/bloom_composite.frag")) {
        OE_WARN("PostProcessor: Failed to load bloom composite shader");
    }
    
    m_blurShader = std::make_shared<Shader>();
    if (!m_blurShader->LoadFromFile("assets/shaders/passes/post/bloom_blur.vert", "assets/shaders/passes/post/bloom_blur.frag")) {
        OE_WARN("PostProcessor: Failed to load blur shader");
    }
    
    m_chromaticAberrationShader = std::make_shared<Shader>();
    if (!m_chromaticAberrationShader->LoadFromFile("assets/shaders/passes/post/chromatic_aberration.vert", "assets/shaders/passes/post/chromatic_aberration.frag")) {
        OE_WARN("PostProcessor: Failed to load chromatic aberration shader");
    }
    
    m_fxaaShader = std::make_shared<Shader>();
    if (!m_fxaaShader->LoadFromFile("assets/shaders/passes/post/fxaa.vert", "assets/shaders/passes/post/fxaa.frag")) {
        OE_WARN("PostProcessor: Failed to load FXAA shader");
    }
    
    m_ssaoShader = std::make_shared<Shader>();
    if (!m_ssaoShader->LoadFromFile("assets/shaders/passes/post/ssao.vert", "assets/shaders/passes/post/ssao.frag")) {
        OE_WARN("PostProcessor: Failed to load SSAO shader");
    }
    
    m_ssrShader = std::make_shared<Shader>();
    if (!m_ssrShader->LoadFromFile("assets/shaders/passes/post/ssr.vert", "assets/shaders/passes/post/ssr.frag")) {
        OE_WARN("PostProcessor: Failed to load SSR shader");
    }
    
    m_motionBlurShader = std::make_shared<Shader>();
    if (!m_motionBlurShader->LoadFromFile("assets/shaders/passes/post/motion_blur.vert", "assets/shaders/passes/post/motion_blur.frag")) {
        OE_WARN("PostProcessor: Failed to load motion blur shader");
    }
    
    m_vignetteShader = std::make_shared<Shader>();
    if (!m_vignetteShader->LoadFromFile("assets/shaders/passes/post/vignette.vert", "assets/shaders/passes/post/vignette.frag")) {
        OE_WARN("PostProcessor: Failed to load vignette shader");
    }
    
    m_filmGrainShader = std::make_shared<Shader>();
    if (!m_filmGrainShader->LoadFromFile("assets/shaders/passes/post/film_grain.vert", "assets/shaders/passes/post/film_grain.frag")) {
        OE_WARN("PostProcessor: Failed to load film grain shader");
    }
}

GLuint PostProcessor::ApplyPostProcessing(GLuint hdrInputTexture, const GBuffer& gbuffer, const Settings& settings) {
    if (!hdrInputTexture || m_outputTexture == 0) {
        return hdrInputTexture; // Fallback to input if not initialized
    }
    
    // SSAO (applied before tone mapping, in linear space)
    GLuint ssaoTexture = 0;
    if (settings.enableSSAO) {
        ssaoTexture = ApplySSAO(gbuffer, settings);
        // SSAO will be applied in lighting pass or blended here
    }
    
    // Bloom extraction and blur (if enabled)
    GLuint bloomTexture = 0;
    if (settings.enableBloom) {
        bloomTexture = ExtractBloom(hdrInputTexture, settings.bloomThreshold);
        bloomTexture = BlurTexture(bloomTexture, settings.bloomIterations);
    }
    
    // Tone mapping
    GLuint toneMappedTexture = ApplyToneMapping(hdrInputTexture, settings);
    
    // Apply SSAO to tone-mapped result (multiply occlusion)
    if (settings.enableSSAO && ssaoTexture != 0) {
        // Blend SSAO with scene (darken based on occlusion)
        glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
        glViewport(0, 0, m_width, m_height);
        glDisable(GL_DEPTH_TEST);
        
        // Use a simple multiply shader or just sample and multiply
        // For now, we'll create a simple SSAO blend
        // TODO: Create dedicated SSAO blend shader
        static std::shared_ptr<Shader> ssaoBlendShader = nullptr;
        if (!ssaoBlendShader) {
            ssaoBlendShader = std::make_shared<Shader>();
            const char* vert = R"(
#version 450 core
layout(location=0) in vec2 a_Position;
layout(location=1) in vec2 a_TexCoord;
out vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";
            const char* frag = R"(
#version 450 core
in vec2 v_TexCoord;
out vec4 FragColor;
uniform sampler2D u_Scene;
uniform sampler2D u_SSAO;
void main() {
    vec3 scene = texture(u_Scene, v_TexCoord).rgb;
    float occlusion = texture(u_SSAO, v_TexCoord).r;
    FragColor = vec4(scene * occlusion, 1.0);
}
)";
            ssaoBlendShader->Compile(vert, frag);
        }
        
        if (ssaoBlendShader && ssaoBlendShader->IsValid()) {
            ssaoBlendShader->Bind();
            ssaoBlendShader->SetInt("u_Scene", 0);
            ssaoBlendShader->SetInt("u_SSAO", 1);
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, toneMappedTexture);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, ssaoTexture);
            
            static FullscreenQuad quad;
            quad.Render();
            
            ssaoBlendShader->Unbind();
            toneMappedTexture = m_outputTexture;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    // Bloom composite (if enabled)
    if (settings.enableBloom && bloomTexture != 0 && m_bloomCompositeShader && m_bloomCompositeShader->IsValid()) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
        glViewport(0, 0, m_width, m_height);
        glDisable(GL_DEPTH_TEST);
        
        m_bloomCompositeShader->Bind();
        m_bloomCompositeShader->SetInt("u_SceneTexture", 0);
        m_bloomCompositeShader->SetInt("u_BloomTexture", 1);
        m_bloomCompositeShader->SetFloat("u_BloomIntensity", settings.bloomIntensity);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, toneMappedTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomTexture);
        
        static FullscreenQuad quad;
        quad.Render();
        
        m_bloomCompositeShader->Unbind();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        toneMappedTexture = m_outputTexture;
    }
    
    // General Blur (if enabled, applied after bloom)
    if (settings.enableBlur) {
        toneMappedTexture = ApplyGeneralBlur(toneMappedTexture, settings.blurIntensity, settings.blurIterations);
    }
    
    // SSR (Screen-Space Reflections) - before chromatic aberration
    if (settings.enableSSR) {
        GLuint skyboxTex = 0; // TODO: Get from RenderPipeline
        toneMappedTexture = ApplySSR(toneMappedTexture, gbuffer, skyboxTex, settings);
    }
    
    // Motion Blur (needs velocity buffer - will implement later)
    // if (settings.enableMotionBlur) {
    //     toneMappedTexture = ApplyMotionBlur(toneMappedTexture, gbuffer, settings);
    // }
    
    // Chromatic Aberration
    if (settings.enableChromaticAberration) {
        toneMappedTexture = ApplyChromaticAberration(toneMappedTexture, settings.chromaticAberrationIntensity);
    }
    
    // Vignette
    if (settings.enableVignette) {
        toneMappedTexture = ApplyVignette(toneMappedTexture, settings);
    }
    
    // Film Grain (before FXAA)
    if (settings.enableFilmGrain) {
        toneMappedTexture = ApplyFilmGrain(toneMappedTexture, settings);
    }
    
    // FXAA (final pass, on LDR)
    if (settings.enableFXAA) {
        toneMappedTexture = ApplyFXAA(toneMappedTexture);
    }
    
    return m_outputTexture; // Final output
}

GLuint PostProcessor::ApplyToneMapping(GLuint hdrTexture, const Settings& settings) {
    if (!m_toneMappingShader || !m_toneMappingShader->IsValid()) {
        return hdrTexture;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    
    m_toneMappingShader->Bind();
    m_toneMappingShader->SetInt("u_HDRTexture", 0);
    m_toneMappingShader->SetFloat("u_Exposure", settings.exposure);
    m_toneMappingShader->SetBool("u_UseACES", settings.useACES);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    
    static FullscreenQuad quad;
    quad.Render();
    
    m_toneMappingShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return m_outputTexture;
}

GLuint PostProcessor::ExtractBloom(GLuint hdrTexture, float threshold) {
    if (!m_bloomExtractShader || !m_bloomExtractShader->IsValid()) {
        return 0;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomExtractFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    
    m_bloomExtractShader->Bind();
    m_bloomExtractShader->SetInt("u_HDRTexture", 0);
    m_bloomExtractShader->SetFloat("u_Threshold", threshold);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    
    static FullscreenQuad quad;
    quad.Render();
    
    m_bloomExtractShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return m_bloomExtractTexture;
}

GLuint PostProcessor::BlurTexture(GLuint texture, int iterations) {
    if (!m_bloomBlurShader || !m_bloomBlurShader->IsValid()) {
        return texture;
    }
    
    bool horizontal = true;
    GLuint currentTexture = texture;
    
    m_bloomBlurShader->Bind();
    m_bloomBlurShader->SetVec2("u_Resolution", glm::vec2(m_width, m_height));
    m_bloomBlurShader->SetFloat("u_Intensity", 1.0f); // Bloom blur uses fixed intensity
    
    for (int i = 0; i < iterations; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_pingPongFBO[horizontal ? 0 : 1]);
        glViewport(0, 0, m_width, m_height);
        glDisable(GL_DEPTH_TEST);
        
        m_bloomBlurShader->SetBool("u_Horizontal", horizontal);
        m_bloomBlurShader->SetInt("u_Texture", 0);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentTexture);
        
        static FullscreenQuad quad;
        quad.Render();
        
        currentTexture = m_pingPongTexture[horizontal ? 0 : 1];
        horizontal = !horizontal;
    }
    
    m_bloomBlurShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return currentTexture;
}

GLuint PostProcessor::ApplyFXAA(GLuint texture) {
    if (!m_fxaaShader || !m_fxaaShader->IsValid()) {
        return texture;
    }
    
    // Use output FBO for FXAA result
    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    
    m_fxaaShader->Bind();
    m_fxaaShader->SetInt("u_Texture", 0);
    m_fxaaShader->SetVec2("u_Resolution", glm::vec2(m_width, m_height));
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    static FullscreenQuad quad;
    quad.Render();
    
    m_fxaaShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return m_outputTexture;
}

GLuint PostProcessor::ApplyGeneralBlur(GLuint texture, float intensity, int iterations) {
    if (!m_blurShader || !m_blurShader->IsValid()) {
        return texture;
    }
    
    bool horizontal = true;
    GLuint currentTexture = texture;
    
    m_blurShader->Bind();
    m_blurShader->SetVec2("u_Resolution", glm::vec2(m_width, m_height));
    m_blurShader->SetFloat("u_Intensity", intensity);
    
    // Apply blur passes with intensity-controlled radius
    for (int i = 0; i < iterations; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_pingPongFBO[horizontal ? 0 : 1]);
        glViewport(0, 0, m_width, m_height);
        glDisable(GL_DEPTH_TEST);
        
        m_blurShader->SetBool("u_Horizontal", horizontal);
        m_blurShader->SetInt("u_Texture", 0);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentTexture);
        
        static FullscreenQuad quad;
        quad.Render();
        
        currentTexture = m_pingPongTexture[horizontal ? 0 : 1];
        horizontal = !horizontal;
    }
    
    m_blurShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return currentTexture;
}

GLuint PostProcessor::ApplyChromaticAberration(GLuint texture, float intensity) {
    if (!m_chromaticAberrationShader || !m_chromaticAberrationShader->IsValid()) {
        return texture;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    
    m_chromaticAberrationShader->Bind();
    m_chromaticAberrationShader->SetInt("u_Texture", 0);
    m_chromaticAberrationShader->SetVec2("u_Resolution", glm::vec2(m_width, m_height));
    m_chromaticAberrationShader->SetFloat("u_Intensity", intensity);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    static FullscreenQuad quad;
    quad.Render();
    
    m_chromaticAberrationShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return m_outputTexture;
}

GLuint PostProcessor::ApplySSAO(const GBuffer& gbuffer, const Settings& settings) {
    if (!m_ssaoShader || !m_ssaoShader->IsValid()) {
        return 0;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    m_ssaoShader->Bind();
    m_ssaoShader->SetInt("g_Position", 0);
    m_ssaoShader->SetInt("g_Normal", 1);
    m_ssaoShader->SetInt("g_Depth", 2);
    m_ssaoShader->SetVec2("u_Resolution", glm::vec2(m_width, m_height));
    m_ssaoShader->SetFloat("u_Radius", settings.ssaoRadius);
    m_ssaoShader->SetFloat("u_Bias", settings.ssaoBias);
    m_ssaoShader->SetInt("u_SampleCount", settings.ssaoSamples);
    
    // FrameUniforms UBO should be bound by caller (RenderPipeline)
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gbuffer.GetPositionTexture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gbuffer.GetNormalTexture());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gbuffer.GetDepthTexture());
    
    static FullscreenQuad quad;
    quad.Render();
    
    m_ssaoShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return m_ssaoTexture;
}

GLuint PostProcessor::ApplySSR(GLuint sceneTexture, const GBuffer& gbuffer, GLuint skyboxTexture, const Settings& settings) {
    if (!m_ssrShader || !m_ssrShader->IsValid()) {
        return sceneTexture;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    
    m_ssrShader->Bind();
    m_ssrShader->SetInt("g_Position", 0);
    m_ssrShader->SetInt("g_Normal", 1);
    m_ssrShader->SetInt("g_Albedo", 2);
    m_ssrShader->SetInt("u_SceneTexture", 3);
    m_ssrShader->SetInt("u_Skybox", 4);
    m_ssrShader->SetVec2("u_Resolution", glm::vec2(m_width, m_height));
    m_ssrShader->SetFloat("u_StepSize", settings.ssrStepSize);
    m_ssrShader->SetInt("u_MaxSteps", settings.ssrMaxSteps);
    m_ssrShader->SetFloat("u_Thickness", settings.ssrThickness);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gbuffer.GetPositionTexture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gbuffer.GetNormalTexture());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gbuffer.GetAlbedoTexture());
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    if (skyboxTexture != 0) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
    }
    
    static FullscreenQuad quad;
    quad.Render();
    
    m_ssrShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return m_outputTexture;
}

GLuint PostProcessor::ApplyMotionBlur(GLuint texture, const GBuffer& gbuffer, const Settings& settings) {
    // Motion blur requires velocity buffer - implement when G-Buffer has velocity
    return texture;
}

GLuint PostProcessor::ApplyVignette(GLuint texture, const Settings& settings) {
    if (!m_vignetteShader || !m_vignetteShader->IsValid()) {
        return texture;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    
    m_vignetteShader->Bind();
    m_vignetteShader->SetInt("u_Texture", 0);
    m_vignetteShader->SetFloat("u_Intensity", settings.vignetteIntensity);
    m_vignetteShader->SetFloat("u_Radius", settings.vignetteRadius);
    m_vignetteShader->SetFloat("u_Softness", settings.vignetteSoftness);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    static FullscreenQuad quad;
    quad.Render();
    
    m_vignetteShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return m_outputTexture;
}

GLuint PostProcessor::ApplyFilmGrain(GLuint texture, const Settings& settings) {
    if (!m_filmGrainShader || !m_filmGrainShader->IsValid()) {
        return texture;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);
    
    m_filmGrainShader->Bind();
    m_filmGrainShader->SetInt("u_Texture", 0);
    m_filmGrainShader->SetFloat("u_Intensity", settings.filmGrainIntensity);
    m_filmGrainShader->SetFloat("u_Time", 0.0f); // TODO: Pass actual time
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    static FullscreenQuad quad;
    quad.Render();
    
    m_filmGrainShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return m_outputTexture;
}

} // namespace Rendering
} // namespace OpenEye

