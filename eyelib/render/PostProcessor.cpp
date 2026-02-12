#include "eyelib/render/PostProcessor.h"
#include "eyelib/render/GBuffer.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/rhi/RHIContext.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/core/Log.h"

namespace eyelib {
namespace render {

namespace {
rhi::VertexLayout GetFullscreenVertexLayout() {
    rhi::VertexLayout layout;
    layout.strideInBytes = 4 * sizeof(float);  // vec2 pos + vec2 uv
    layout.attributes = {
        {0, rhi::VertexAttributeType::Float2, 0},
        {1, rhi::VertexAttributeType::Float2, 2 * sizeof(float)},
    };
    return layout;
}
} // namespace

PostProcessor::PostProcessor() = default;

PostProcessor::~PostProcessor() {
    Shutdown();
}

void PostProcessor::Shutdown() {
    DestroyFullscreenQuad();
    DestroyFramebuffers();
}

void PostProcessor::DestroyFullscreenQuad() {
    if (!m_deviceRHI) return;
    if (m_fullscreenVBO) { m_deviceRHI->DestroyBuffer(m_fullscreenVBO); m_fullscreenVBO = nullptr; }
    if (m_fullscreenIBO) { m_deviceRHI->DestroyBuffer(m_fullscreenIBO); m_fullscreenIBO = nullptr; }
}

void PostProcessor::CreateFullscreenQuad() {
    if (!m_deviceRHI) return;
    DestroyFullscreenQuad();
    // Fullscreen quad: vec2 pos (location 0) + vec2 uv (location 1), matches post shaders
    const float vertices[] = {
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,
    };
    const uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
    rhi::BufferDesc vboDesc;
    vboDesc.sizeInBytes = sizeof(vertices);
    vboDesc.usage = rhi::BufferUsage::Vertex;
    m_fullscreenVBO = m_deviceRHI->CreateBuffer(vboDesc, vertices);
    rhi::BufferDesc iboDesc;
    iboDesc.sizeInBytes = sizeof(indices);
    iboDesc.usage = rhi::BufferUsage::Index;
    m_fullscreenIBO = m_deviceRHI->CreateBuffer(iboDesc, indices);
}

void PostProcessor::DrawFullscreenQuad(rhi::ICommandList* cmd) {
    if (!cmd || !m_fullscreenVBO || !m_fullscreenIBO) return;
    cmd->BindVertexBuffer(m_fullscreenVBO, 0);
    cmd->BindIndexBuffer(m_fullscreenIBO, rhi::IndexType::UInt16);
    cmd->DrawIndexed(6, 0, 0);
}

void PostProcessor::DestroyFramebuffers() {
    if (!m_deviceRHI) return;
    for (auto& rt : m_pingPongRT) {
        if (rt) {
            m_deviceRHI->DestroyRenderTarget(rt);
            rt = nullptr;
        }
    }
    if (m_outputRT) { m_deviceRHI->DestroyRenderTarget(m_outputRT); m_outputRT = nullptr; }
    if (m_bloomExtractRT) { m_deviceRHI->DestroyRenderTarget(m_bloomExtractRT); m_bloomExtractRT = nullptr; }
    if (m_ssaoRT) { m_deviceRHI->DestroyRenderTarget(m_ssaoRT); m_ssaoRT = nullptr; }
}

bool PostProcessor::Initialize(uint32_t width, uint32_t height, rhi::IDevice* device) {
    m_width = width;
    m_height = height;
    m_lastOutputValid = false;
    if (device) m_deviceRHI = device;
    if (!m_deviceRHI) m_deviceRHI = rhi::GetDefaultDevice();
    if (!m_deviceRHI) {
        core::Log::Error("PostProcessor::Initialize: no RHI device");
        return false;
    }
    core::Log::Info("PostProcessor::Initialize: loading shaders");
    LoadShaders();
    core::Log::Info("PostProcessor::Initialize: creating fullscreen quad");
    CreateFullscreenQuad();
    if (!m_fullscreenVBO || !m_fullscreenIBO) {
        core::Log::Error("PostProcessor::Initialize: CreateFullscreenQuad failed (VBO or IBO null)");
        return false;
    }
    core::Log::Info("PostProcessor::Initialize: creating framebuffers");
    CreateFramebuffers();
    rhi::ITexture* outTex = GetOutputTextureRHI();
    if (!m_outputRT) {
        core::Log::Error("PostProcessor::Initialize: m_outputRT is null after CreateFramebuffers");
        return false;
    }
    if (!outTex) {
        core::Log::Error("PostProcessor::Initialize: GetOutputTextureRHI() returned null (no color texture on output RT)");
        return false;
    }
    return true;
}

void PostProcessor::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    if (width == 0 || height == 0) {
        m_width = width;
        m_height = height;
        m_lastOutputValid = false;
        return;
    }
    m_width = width;
    m_height = height;
    m_lastOutputValid = false;
    DestroyFramebuffers();
    CreateFramebuffers();
}

void PostProcessor::CreateFramebuffers() {
    if (m_width == 0 || m_height == 0) {
        return;
    }
    if (!m_deviceRHI) return;
    std::vector<rhi::RHITextureFormat> rgb16f = { rhi::RHITextureFormat::RGB16F };
    std::vector<rhi::RHITextureFormat> rgba8 = { rhi::RHITextureFormat::RGBA8 };
    std::vector<rhi::RHITextureFormat> r8 = { rhi::RHITextureFormat::R8 };

    m_pingPongRT[0] = m_deviceRHI->CreateRenderTarget(m_width, m_height, rgb16f, rhi::RHITextureFormat::None, 1);
    if (!m_pingPongRT[0]) core::Log::Error("PostProcessor::CreateFramebuffers: pingPongRT[0] failed");
    m_pingPongRT[1] = m_deviceRHI->CreateRenderTarget(m_width, m_height, rgb16f, rhi::RHITextureFormat::None, 1);
    if (!m_pingPongRT[1]) core::Log::Error("PostProcessor::CreateFramebuffers: pingPongRT[1] failed");
    m_bloomExtractRT = m_deviceRHI->CreateRenderTarget(m_width, m_height, rgb16f, rhi::RHITextureFormat::None, 1);
    if (!m_bloomExtractRT) core::Log::Error("PostProcessor::CreateFramebuffers: bloomExtractRT failed");
    m_outputRT = m_deviceRHI->CreateRenderTarget(m_width, m_height, rgba8, rhi::RHITextureFormat::None, 1);
    if (!m_outputRT) core::Log::Error("PostProcessor::CreateFramebuffers: outputRT failed");
    m_ssaoRT = m_deviceRHI->CreateRenderTarget(m_width, m_height, r8, rhi::RHITextureFormat::None, 1);
    if (!m_ssaoRT) core::Log::Error("PostProcessor::CreateFramebuffers: ssaoRT failed");
}

void PostProcessor::LoadShaders() {
    if (!m_deviceRHI) return;
    const rhi::VertexLayout fullscreenLayout = GetFullscreenVertexLayout();

    m_toneMappingShader = std::make_shared<Shader>();
    if (!m_toneMappingShader->Load(m_deviceRHI, "passes/post/tonemap", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load tone mapping shader");
    }

    m_bloomExtractShader = std::make_shared<Shader>();
    if (!m_bloomExtractShader->Load(m_deviceRHI, "passes/post/bloom_extract", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load bloom extract shader");
    }

    m_bloomBlurShader = std::make_shared<Shader>();
    if (!m_bloomBlurShader->Load(m_deviceRHI, "passes/post/bloom_blur", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load bloom blur shader");
    }

    m_bloomCompositeShader = std::make_shared<Shader>();
    if (!m_bloomCompositeShader->Load(m_deviceRHI, "passes/post/bloom_composite", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load bloom composite shader");
    }

    m_blurShader = std::make_shared<Shader>();
    if (!m_blurShader->Load(m_deviceRHI, "passes/post/bloom_blur", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load blur shader");
    }

    m_chromaticAberrationShader = std::make_shared<Shader>();
    if (!m_chromaticAberrationShader->Load(m_deviceRHI, "passes/post/chromatic_aberration", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load chromatic aberration shader");
    }

    m_fxaaShader = std::make_shared<Shader>();
    if (!m_fxaaShader->Load(m_deviceRHI, "passes/post/fxaa", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load FXAA shader");
    }

    m_ssaoShader = std::make_shared<Shader>();
    if (!m_ssaoShader->Load(m_deviceRHI, "passes/post/ssao", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load SSAO shader");
    }

    m_ssrShader = std::make_shared<Shader>();
    if (!m_ssrShader->Load(m_deviceRHI, "passes/post/ssr", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load SSR shader");
    }

    m_motionBlurShader = std::make_shared<Shader>();
    if (!m_motionBlurShader->Load(m_deviceRHI, "passes/post/motion_blur", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load motion blur shader");
    }

    m_vignetteShader = std::make_shared<Shader>();
    if (!m_vignetteShader->Load(m_deviceRHI, "passes/post/vignette", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load vignette shader");
    }

    m_filmGrainShader = std::make_shared<Shader>();
    if (!m_filmGrainShader->Load(m_deviceRHI, "passes/post/film_grain", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load film grain shader");
    }

    // SSAO blend shader (simple multiply) - same fullscreen layout
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
    m_ssaoBlendShader->BuildFromSource(m_deviceRHI, ssaoBlendVert, ssaoBlendFrag, fullscreenLayout);
}

rhi::ITexture* PostProcessor::ApplyPostProcessing(rhi::ITexture* hdrInputTexture, const GBuffer& gbuffer, const Settings& settings,
                                                  const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                                  const math::Mat4& invViewMatrix, const math::Mat4& invProjectionMatrix,
                                                  const math::Vec3& viewPos, rhi::ITexture* skyboxTexture, float time) {
    m_lastOutputValid = false;
    if (!hdrInputTexture || !GetOutputTextureRHI()) {
        return hdrInputTexture;
    }
    rhi::ICommandList* cmdPost = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (cmdPost)
        rhi::BarrierTextureForShaderRead(cmdPost, hdrInputTexture);

    // SSAO (applied before tone mapping, in linear space)
    rhi::ITexture* ssaoTexture = nullptr;
    if (settings.enableSSAO) {
        ssaoTexture = ApplySSAO(gbuffer, settings);
    }

    // Bloom extraction and blur (if enabled)
    rhi::ITexture* bloomTexture = nullptr;
    if (settings.enableBloom) {
        bloomTexture = ExtractBloom(hdrInputTexture, settings.bloomThreshold);
        bloomTexture = BlurTexture(bloomTexture, settings.bloomIterations);
    }

    // Tone mapping
    rhi::ITexture* toneMappedTexture = ApplyToneMapping(hdrInputTexture, settings);

    // Apply SSAO to tone-mapped result (multiply occlusion)
    if (settings.enableSSAO && ssaoTexture && m_ssaoBlendShader && m_ssaoBlendShader->IsValid()) {
        rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
        if (cmd && m_outputRT) {
            cmd->SetRenderTarget(m_outputRT);
            cmd->SetViewport(0, 0, m_width, m_height);
            cmd->SetDepthTest(false);
            cmd->SetDepthWrite(false);
            cmd->SetCullFace(false);
            cmd->SetBlend(false);
            m_ssaoBlendShader->Bind();
            m_ssaoBlendShader->SetInt("u_Scene", 0);
            m_ssaoBlendShader->SetInt("u_SSAO", 1);
            cmd->BindTexture(toneMappedTexture, 0);
            cmd->BindTexture(ssaoTexture, 1);
            DrawFullscreenQuad(cmd);
            m_ssaoBlendShader->Unbind();
            toneMappedTexture = GetOutputTextureRHI();
        }
    }

    // Bloom composite (if enabled)
    if (settings.enableBloom && bloomTexture && m_bloomCompositeShader && m_bloomCompositeShader->IsValid()) {
        rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
        if (cmd && m_outputRT) {
            rhi::BarrierTextureForShaderRead(cmd, toneMappedTexture);
            rhi::BarrierTextureForShaderRead(cmd, bloomTexture);
            cmd->SetRenderTarget(m_outputRT);
            cmd->SetViewport(0, 0, m_width, m_height);
            cmd->SetDepthTest(false);
            cmd->SetDepthWrite(false);
            cmd->SetCullFace(false);
            cmd->SetBlend(false);
            m_bloomCompositeShader->Bind();
            m_bloomCompositeShader->SetInt("u_SceneTexture", 0);
            m_bloomCompositeShader->SetInt("u_BloomTexture", 1);
            m_bloomCompositeShader->SetFloat("u_BloomIntensity", settings.bloomIntensity);
            cmd->BindTexture(toneMappedTexture, 0);
            cmd->BindTexture(bloomTexture, 1);
            DrawFullscreenQuad(cmd);
            m_bloomCompositeShader->Unbind();
            toneMappedTexture = GetOutputTextureRHI();
        }
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
        rhi::ITexture* velocityTex = gbuffer.GetVelocityTextureRHI();
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

    return GetOutputTextureRHI();
}

 rhi::ITexture* PostProcessor::ApplyToneMapping(rhi::ITexture* hdrTexture, const Settings& settings) {
    if (!m_toneMappingShader || !m_toneMappingShader->IsValid()) {
        return hdrTexture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (!cmd || !m_outputRT) return hdrTexture;
    cmd->SetRenderTarget(m_outputRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);

    rhi::ClearValue clearBlack = {0.f, 0.f, 0.f, 1.f};
    cmd->ClearRenderTarget(m_outputRT, clearBlack);

    m_toneMappingShader->Bind();
    m_toneMappingShader->SetInt("u_HDRTexture", 0);
    m_toneMappingShader->SetFloat("u_Exposure", settings.exposure);
    m_toneMappingShader->SetBool("u_UseACES", settings.useACES);

    cmd->BindTexture(hdrTexture, 0);
    DrawFullscreenQuad(cmd);
    m_lastOutputValid = true;
    m_toneMappingShader->Unbind();
    return GetOutputTextureRHI();
}

 rhi::ITexture* PostProcessor::ExtractBloom(rhi::ITexture* hdrTexture, float threshold) {
    if (!m_bloomExtractShader || !m_bloomExtractShader->IsValid()) {
        return nullptr;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (!cmd || !m_bloomExtractRT) return nullptr;
    cmd->SetRenderTarget(m_bloomExtractRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);

    m_bloomExtractShader->Bind();
    m_bloomExtractShader->SetInt("u_HDRTexture", 0);
    m_bloomExtractShader->SetFloat("u_Threshold", threshold);
    cmd->BindTexture(hdrTexture, 0);
    DrawFullscreenQuad(cmd);
    m_bloomExtractShader->Unbind();
    rhi::BarrierTextureForShaderRead(cmd, GetColorTexture(m_bloomExtractRT));
    return GetColorTexture(m_bloomExtractRT);
}

 rhi::ITexture* PostProcessor::BlurTexture(rhi::ITexture* texture, int iterations) {
    if (!m_bloomBlurShader || !m_bloomBlurShader->IsValid()) {
        return texture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (!cmd) return texture;
    bool horizontal = true;
    rhi::ITexture* currentTexture = texture;

    m_bloomBlurShader->Bind();
    m_bloomBlurShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_bloomBlurShader->SetFloat("u_Intensity", 1.0f);

    for (int i = 0; i < iterations; ++i) {
        rhi::IRenderTarget* rt = m_pingPongRT[horizontal ? 0 : 1];
        if (!rt) break;
        if (i > 0)
            rhi::BarrierTextureForShaderRead(cmd, currentTexture);
        cmd->SetRenderTarget(rt);
        cmd->SetViewport(0, 0, m_width, m_height);
        cmd->SetDepthTest(false);
        cmd->SetDepthWrite(false);
        cmd->SetCullFace(false);
        cmd->SetBlend(false);
        m_bloomBlurShader->SetBool("u_Horizontal", horizontal);
        m_bloomBlurShader->SetInt("u_Texture", 0);
        cmd->BindTexture(currentTexture, 0);
        DrawFullscreenQuad(cmd);
        currentTexture = GetColorTexture(rt);
        horizontal = !horizontal;
    }

    m_bloomBlurShader->Unbind();
    return currentTexture;
}

 rhi::ITexture* PostProcessor::ApplyGeneralBlur(rhi::ITexture* texture, float intensity, int iterations) {
    if (!m_blurShader || !m_blurShader->IsValid()) {
        return texture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (!cmd) return texture;
    bool horizontal = true;
    rhi::ITexture* currentTexture = texture;

    m_blurShader->Bind();
    m_blurShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_blurShader->SetFloat("u_Intensity", intensity);

    for (int i = 0; i < iterations; ++i) {
        rhi::IRenderTarget* rt = m_pingPongRT[horizontal ? 0 : 1];
        if (!rt) break;
        cmd->SetRenderTarget(rt);
        cmd->SetViewport(0, 0, m_width, m_height);
        cmd->SetDepthTest(false);
        cmd->SetDepthWrite(false);
        cmd->SetCullFace(false);
        cmd->SetBlend(false);
        m_blurShader->SetBool("u_Horizontal", horizontal);
        m_blurShader->SetInt("u_Texture", 0);
        cmd->BindTexture(currentTexture, 0);
        DrawFullscreenQuad(cmd);
        currentTexture = GetColorTexture(rt);
        horizontal = !horizontal;
    }

    m_blurShader->Unbind();
    return currentTexture;
}

 rhi::ITexture* PostProcessor::ApplyChromaticAberration(rhi::ITexture* texture, float intensity) {
    if (!m_chromaticAberrationShader || !m_chromaticAberrationShader->IsValid()) {
        return texture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (!cmd || !m_outputRT) return texture;
    cmd->SetRenderTarget(m_outputRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);
    m_chromaticAberrationShader->Bind();
    m_chromaticAberrationShader->SetInt("u_Texture", 0);
    m_chromaticAberrationShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_chromaticAberrationShader->SetFloat("u_Intensity", intensity);
    cmd->BindTexture(texture, 0);
    DrawFullscreenQuad(cmd);
    m_chromaticAberrationShader->Unbind();
    return GetOutputTextureRHI();
}

 rhi::ITexture* PostProcessor::ApplyFXAA(rhi::ITexture* texture) {
    if (!m_fxaaShader || !m_fxaaShader->IsValid()) {
        return texture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (!cmd || !m_outputRT) return texture;
    cmd->SetRenderTarget(m_outputRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);
    m_fxaaShader->Bind();
    m_fxaaShader->SetInt("u_Texture", 0);
    m_fxaaShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    cmd->BindTexture(texture, 0);
    DrawFullscreenQuad(cmd);
    m_fxaaShader->Unbind();
    return GetOutputTextureRHI();
}

 rhi::ITexture* PostProcessor::ApplySSAO(const GBuffer& gbuffer, const Settings& settings) {
    if (!m_ssaoShader || !m_ssaoShader->IsValid()) {
        return nullptr;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (!cmd || !m_ssaoRT) return nullptr;
    rhi::ClearValue cv = {1.f, 1.f, 1.f, 1.f};
    rhi::ClearAndSetRenderTargetWithViewport(cmd, m_ssaoRT, 0, 0, m_width, m_height, cv);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);

    m_ssaoShader->Bind();
    m_ssaoShader->SetInt("g_Position", 0);
    m_ssaoShader->SetInt("g_Normal", 1);
    m_ssaoShader->SetInt("g_Depth", 2);
    m_ssaoShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_ssaoShader->SetFloat("u_Radius", settings.ssaoRadius);
    m_ssaoShader->SetFloat("u_Bias", settings.ssaoBias);
    m_ssaoShader->SetInt("u_SampleCount", settings.ssaoSamples);
    cmd->BindTexture(gbuffer.GetPositionTextureRHI(), 0);
    cmd->BindTexture(gbuffer.GetNormalTextureRHI(), 1);
    cmd->BindTexture(gbuffer.GetDepthTextureRHI(), 2);
    DrawFullscreenQuad(cmd);
    m_ssaoShader->Unbind();
    return GetColorTexture(m_ssaoRT);
}

 rhi::ITexture* PostProcessor::ApplySSR(rhi::ITexture* sceneTexture, const GBuffer& gbuffer, rhi::ITexture* skyboxTexture, const Settings& settings,
                                       const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                       const math::Mat4& invViewMatrix, const math::Mat4& invProjectionMatrix,
                                       const math::Vec3& viewPos) {
    if (!m_ssrShader || !m_ssrShader->IsValid()) {
        return sceneTexture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (!cmd || !m_outputRT) return sceneTexture;
    cmd->SetRenderTarget(m_outputRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);

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
    cmd->BindTexture(gbuffer.GetPositionTextureRHI(), 0);
    cmd->BindTexture(gbuffer.GetNormalTextureRHI(), 1);
    cmd->BindTexture(gbuffer.GetAlbedoTextureRHI(), 2);
    cmd->BindTexture(sceneTexture, 3);
    if (skyboxTexture) cmd->BindTexture(skyboxTexture, 4);
    DrawFullscreenQuad(cmd);
    m_ssrShader->Unbind();
    return GetOutputTextureRHI();
}

 rhi::ITexture* PostProcessor::ApplyMotionBlur(rhi::ITexture* texture, rhi::ITexture* velocityTexture, const Settings& settings) {
    if (!m_motionBlurShader || !m_motionBlurShader->IsValid() || !velocityTexture) {
        return texture;  // Skip if no shader or no velocity buffer
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (!cmd || !m_outputRT) return texture;
    cmd->SetRenderTarget(m_outputRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);

    m_motionBlurShader->Bind();
    m_motionBlurShader->SetInt("u_Texture", 0);
    m_motionBlurShader->SetInt("g_Velocity", 1);
    m_motionBlurShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_motionBlurShader->SetFloat("u_Intensity", settings.motionBlurIntensity);
    m_motionBlurShader->SetInt("u_Samples", settings.motionBlurSamples);
    cmd->BindTexture(texture, 0);
    cmd->BindTexture(velocityTexture, 1);
    DrawFullscreenQuad(cmd);
    m_motionBlurShader->Unbind();
    return GetOutputTextureRHI();
}

 rhi::ITexture* PostProcessor::ApplyVignette(rhi::ITexture* texture, const Settings& settings) {
    if (!m_vignetteShader || !m_vignetteShader->IsValid()) {
        return texture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (!cmd || !m_outputRT) return texture;
    cmd->SetRenderTarget(m_outputRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);
    m_vignetteShader->Bind();
    m_vignetteShader->SetInt("u_Texture", 0);
    m_vignetteShader->SetFloat("u_Intensity", settings.vignetteIntensity);
    m_vignetteShader->SetFloat("u_Radius", settings.vignetteRadius);
    m_vignetteShader->SetFloat("u_Softness", settings.vignetteSoftness);
    cmd->BindTexture(texture, 0);
    DrawFullscreenQuad(cmd);
    m_vignetteShader->Unbind();
    return GetOutputTextureRHI();
}

 rhi::ITexture* PostProcessor::ApplyFilmGrain(rhi::ITexture* texture, const Settings& settings, float time) {
    if (!m_filmGrainShader || !m_filmGrainShader->IsValid()) {
        return texture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetImmediateCommandList() : nullptr;
    if (!cmd || !m_outputRT) return texture;
    cmd->SetRenderTarget(m_outputRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);
    m_filmGrainShader->Bind();
    m_filmGrainShader->SetInt("u_Texture", 0);
    m_filmGrainShader->SetFloat("u_Intensity", settings.filmGrainIntensity);
    m_filmGrainShader->SetFloat("u_Time", time);
    cmd->BindTexture(texture, 0);
    DrawFullscreenQuad(cmd);
    m_filmGrainShader->Unbind();
    return GetOutputTextureRHI();
}

rhi::ITexture* PostProcessor::GetColorTexture(rhi::IRenderTarget* rt) const {
    return rt ? rt->GetColorTexture(0) : nullptr;
}

rhi::ITexture* PostProcessor::GetOutputTextureRHI() const {
    return GetColorTexture(m_outputRT);
}

} // namespace render
} // namespace eyelib
