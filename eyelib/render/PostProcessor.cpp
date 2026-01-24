#include "eyelib/render/PostProcessor.h"
#include "eyelib/render/GBuffer.h"
#include "eyelib/render/FullscreenQuad.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/core/Log.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {

PostProcessor::PostProcessor() = default;

PostProcessor::~PostProcessor() {
    Shutdown();
}

void PostProcessor::Shutdown() {
    DestroyFramebuffers();
}

void PostProcessor::DestroyFramebuffers() {
    if (m_pingPongFBO[0]) { glDeleteFramebuffers(1, &m_pingPongFBO[0]); m_pingPongFBO[0] = 0; }
    if (m_pingPongFBO[1]) { glDeleteFramebuffers(1, &m_pingPongFBO[1]); m_pingPongFBO[1] = 0; }
    if (m_pingPongTexture[0]) { glDeleteTextures(1, &m_pingPongTexture[0]); m_pingPongTexture[0] = 0; }
    if (m_pingPongTexture[1]) { glDeleteTextures(1, &m_pingPongTexture[1]); m_pingPongTexture[1] = 0; }
    if (m_outputFBO) { glDeleteFramebuffers(1, &m_outputFBO); m_outputFBO = 0; }
    if (m_outputTexture) { glDeleteTextures(1, &m_outputTexture); m_outputTexture = 0; }
    if (m_bloomExtractFBO) { glDeleteFramebuffers(1, &m_bloomExtractFBO); m_bloomExtractFBO = 0; }
    if (m_bloomExtractTexture) { glDeleteTextures(1, &m_bloomExtractTexture); m_bloomExtractTexture = 0; }
    if (m_ssaoFBO) { glDeleteFramebuffers(1, &m_ssaoFBO); m_ssaoFBO = 0; }
    if (m_ssaoTexture) { glDeleteTextures(1, &m_ssaoTexture); m_ssaoTexture = 0; }
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
            core::Log::ErrorF("PostProcessor: Ping-pong FBO %d incomplete: 0x%x", i, status);
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
        core::Log::WarnF("PostProcessor: Failed to load tone mapping shader");
    }

    m_bloomExtractShader = std::make_shared<Shader>();
    if (!m_bloomExtractShader->LoadFromFile("assets/shaders/passes/post/bloom_extract.vert", "assets/shaders/passes/post/bloom_extract.frag")) {
        core::Log::WarnF("PostProcessor: Failed to load bloom extract shader");
    }

    m_bloomBlurShader = std::make_shared<Shader>();
    if (!m_bloomBlurShader->LoadFromFile("assets/shaders/passes/post/bloom_blur.vert", "assets/shaders/passes/post/bloom_blur.frag")) {
        core::Log::WarnF("PostProcessor: Failed to load bloom blur shader");
    }

    m_bloomCompositeShader = std::make_shared<Shader>();
    if (!m_bloomCompositeShader->LoadFromFile("assets/shaders/passes/post/bloom_composite.vert", "assets/shaders/passes/post/bloom_composite.frag")) {
        core::Log::WarnF("PostProcessor: Failed to load bloom composite shader");
    }

    m_blurShader = std::make_shared<Shader>();
    if (!m_blurShader->LoadFromFile("assets/shaders/passes/post/bloom_blur.vert", "assets/shaders/passes/post/bloom_blur.frag")) {
        core::Log::WarnF("PostProcessor: Failed to load blur shader");
    }

    m_chromaticAberrationShader = std::make_shared<Shader>();
    if (!m_chromaticAberrationShader->LoadFromFile("assets/shaders/passes/post/chromatic_aberration.vert", "assets/shaders/passes/post/chromatic_aberration.frag")) {
        core::Log::WarnF("PostProcessor: Failed to load chromatic aberration shader");
    }

    m_fxaaShader = std::make_shared<Shader>();
    if (!m_fxaaShader->LoadFromFile("assets/shaders/passes/post/fxaa.vert", "assets/shaders/passes/post/fxaa.frag")) {
        core::Log::WarnF("PostProcessor: Failed to load FXAA shader");
    }

    m_ssaoShader = std::make_shared<Shader>();
    if (!m_ssaoShader->LoadFromFile("assets/shaders/passes/post/ssao.vert", "assets/shaders/passes/post/ssao.frag")) {
        core::Log::WarnF("PostProcessor: Failed to load SSAO shader");
    }

    m_ssrShader = std::make_shared<Shader>();
    if (!m_ssrShader->LoadFromFile("assets/shaders/passes/post/ssr.vert", "assets/shaders/passes/post/ssr.frag")) {
        core::Log::WarnF("PostProcessor: Failed to load SSR shader");
    }

    m_motionBlurShader = std::make_shared<Shader>();
    if (!m_motionBlurShader->LoadFromFile("assets/shaders/passes/post/motion_blur.vert", "assets/shaders/passes/post/motion_blur.frag")) {
        core::Log::WarnF("PostProcessor: Failed to load motion blur shader");
    }

    m_vignetteShader = std::make_shared<Shader>();
    if (!m_vignetteShader->LoadFromFile("assets/shaders/passes/post/vignette.vert", "assets/shaders/passes/post/vignette.frag")) {
        core::Log::WarnF("PostProcessor: Failed to load vignette shader");
    }

    m_filmGrainShader = std::make_shared<Shader>();
    if (!m_filmGrainShader->LoadFromFile("assets/shaders/passes/post/film_grain.vert", "assets/shaders/passes/post/film_grain.frag")) {
        core::Log::WarnF("PostProcessor: Failed to load film grain shader");
    }

    // SSAO blend shader (simple multiply)
    m_ssaoBlendShader = std::make_shared<Shader>();
    const char* ssaoBlendVert = R"(
#version 430 core
layout(location=0) in vec2 a_Position;
layout(location=1) in vec2 a_TexCoord;
out vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";
    const char* ssaoBlendFrag = R"(
#version 430 core
in vec2 v_TexCoord;
layout(location=0) out vec4 o_Color;
uniform sampler2D u_Scene;
uniform sampler2D u_SSAO;
void main() {
    vec3 scene = texture(u_Scene, v_TexCoord).rgb;
    float occlusion = texture(u_SSAO, v_TexCoord).r;
    o_Color = vec4(scene * occlusion, 1.0);
}
)";
    m_ssaoBlendShader->BuildFromSource(ssaoBlendVert, ssaoBlendFrag);
}

unsigned int PostProcessor::ApplyPostProcessing(unsigned int hdrInputTexture, const GBuffer& gbuffer, const Settings& settings,
                                                 const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                                 const math::Mat4& invViewMatrix, const math::Mat4& invProjectionMatrix,
                                                 const math::Vec3& viewPos, unsigned int skyboxTexture, float time) {
    if (!hdrInputTexture || m_outputTexture == 0) {
        return hdrInputTexture;  // Fallback to input if not initialized
    }

    // SSAO (applied before tone mapping, in linear space)
    unsigned int ssaoTexture = 0;
    if (settings.enableSSAO) {
        ssaoTexture = ApplySSAO(gbuffer, settings);
    }

    // Bloom extraction and blur (if enabled)
    unsigned int bloomTexture = 0;
    if (settings.enableBloom) {
        bloomTexture = ExtractBloom(hdrInputTexture, settings.bloomThreshold);
        bloomTexture = BlurTexture(bloomTexture, settings.bloomIterations);
    }

    // Tone mapping
    unsigned int toneMappedTexture = ApplyToneMapping(hdrInputTexture, settings);

    // Apply SSAO to tone-mapped result (multiply occlusion)
    if (settings.enableSSAO && ssaoTexture != 0 && m_ssaoBlendShader && m_ssaoBlendShader->IsValid()) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
        glViewport(0, 0, m_width, m_height);
        glDisable(GL_DEPTH_TEST);

        m_ssaoBlendShader->Bind();
        m_ssaoBlendShader->SetInt("u_Scene", 0);
        m_ssaoBlendShader->SetInt("u_SSAO", 1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, toneMappedTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, ssaoTexture);

        static FullscreenQuad quad;
        quad.Render();

        m_ssaoBlendShader->Unbind();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        toneMappedTexture = m_outputTexture;
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
        toneMappedTexture = ApplySSR(toneMappedTexture, gbuffer, skyboxTexture, settings,
                                      viewMatrix, projectionMatrix, invViewMatrix, invProjectionMatrix, viewPos);
    }

    // Motion Blur (requires velocity buffer texture)
    if (settings.enableMotionBlur) {
        unsigned int velocityTex = gbuffer.GetVelocityTexture();
        toneMappedTexture = ApplyMotionBlur(toneMappedTexture, velocityTex, settings);
    }

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
        toneMappedTexture = ApplyFilmGrain(toneMappedTexture, settings, time);
    }

    // FXAA (final pass, on LDR)
    if (settings.enableFXAA) {
        toneMappedTexture = ApplyFXAA(toneMappedTexture);
    }

    return m_outputTexture;  // Final output
}

unsigned int PostProcessor::ApplyToneMapping(unsigned int hdrTexture, const Settings& settings) {
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

unsigned int PostProcessor::ExtractBloom(unsigned int hdrTexture, float threshold) {
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

unsigned int PostProcessor::BlurTexture(unsigned int texture, int iterations) {
    if (!m_bloomBlurShader || !m_bloomBlurShader->IsValid()) {
        return texture;
    }

    bool horizontal = true;
    unsigned int currentTexture = texture;

    m_bloomBlurShader->Bind();
    m_bloomBlurShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_bloomBlurShader->SetFloat("u_Intensity", 1.0f);  // Bloom blur uses fixed intensity

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

unsigned int PostProcessor::ApplyGeneralBlur(unsigned int texture, float intensity, int iterations) {
    if (!m_blurShader || !m_blurShader->IsValid()) {
        return texture;
    }

    bool horizontal = true;
    unsigned int currentTexture = texture;

    m_blurShader->Bind();
    m_blurShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_blurShader->SetFloat("u_Intensity", intensity);

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

unsigned int PostProcessor::ApplyChromaticAberration(unsigned int texture, float intensity) {
    if (!m_chromaticAberrationShader || !m_chromaticAberrationShader->IsValid()) {
        return texture;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);

    m_chromaticAberrationShader->Bind();
    m_chromaticAberrationShader->SetInt("u_Texture", 0);
    m_chromaticAberrationShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_chromaticAberrationShader->SetFloat("u_Intensity", intensity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    static FullscreenQuad quad;
    quad.Render();

    m_chromaticAberrationShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return m_outputTexture;
}

unsigned int PostProcessor::ApplyFXAA(unsigned int texture) {
    if (!m_fxaaShader || !m_fxaaShader->IsValid()) {
        return texture;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);

    m_fxaaShader->Bind();
    m_fxaaShader->SetInt("u_Texture", 0);
    m_fxaaShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    static FullscreenQuad quad;
    quad.Render();

    m_fxaaShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return m_outputTexture;
}

unsigned int PostProcessor::ApplySSAO(const GBuffer& gbuffer, const Settings& settings) {
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
    m_ssaoShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_ssaoShader->SetFloat("u_Radius", settings.ssaoRadius);
    m_ssaoShader->SetFloat("u_Bias", settings.ssaoBias);
    m_ssaoShader->SetInt("u_SampleCount", settings.ssaoSamples);

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

unsigned int PostProcessor::ApplySSR(unsigned int sceneTexture, const GBuffer& gbuffer, unsigned int skyboxTexture, const Settings& settings,
                                      const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                      const math::Mat4& invViewMatrix, const math::Mat4& invProjectionMatrix,
                                      const math::Vec3& viewPos) {
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
    m_ssrShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_ssrShader->SetFloat("u_StepSize", settings.ssrStepSize);
    m_ssrShader->SetInt("u_MaxSteps", settings.ssrMaxSteps);
    m_ssrShader->SetFloat("u_Thickness", settings.ssrThickness);
    m_ssrShader->SetMat4("u_ViewMatrix", viewMatrix);
    m_ssrShader->SetMat4("u_ProjectionMatrix", projectionMatrix);
    m_ssrShader->SetMat4("u_InverseViewMatrix", invViewMatrix);
    m_ssrShader->SetMat4("u_InverseProjectionMatrix", invProjectionMatrix);
    m_ssrShader->SetVec3("u_ViewPos", viewPos);

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

unsigned int PostProcessor::ApplyMotionBlur(unsigned int texture, unsigned int velocityTexture, const Settings& settings) {
    if (!m_motionBlurShader || !m_motionBlurShader->IsValid() || velocityTexture == 0) {
        return texture;  // Skip if no shader or no velocity buffer
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);

    m_motionBlurShader->Bind();
    m_motionBlurShader->SetInt("u_Texture", 0);
    m_motionBlurShader->SetInt("g_Velocity", 1);
    m_motionBlurShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_motionBlurShader->SetFloat("u_Intensity", settings.motionBlurIntensity);
    m_motionBlurShader->SetInt("u_Samples", settings.motionBlurSamples);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, velocityTexture);

    static FullscreenQuad quad;
    quad.Render();

    m_motionBlurShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return m_outputTexture;
}

unsigned int PostProcessor::ApplyVignette(unsigned int texture, const Settings& settings) {
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

unsigned int PostProcessor::ApplyFilmGrain(unsigned int texture, const Settings& settings, float time) {
    if (!m_filmGrainShader || !m_filmGrainShader->IsValid()) {
        return texture;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_DEPTH_TEST);

    m_filmGrainShader->Bind();
    m_filmGrainShader->SetInt("u_Texture", 0);
    m_filmGrainShader->SetFloat("u_Intensity", settings.filmGrainIntensity);
    m_filmGrainShader->SetFloat("u_Time", time);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    static FullscreenQuad quad;
    quad.Render();

    m_filmGrainShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return m_outputTexture;
}

} // namespace render
} // namespace eyelib
