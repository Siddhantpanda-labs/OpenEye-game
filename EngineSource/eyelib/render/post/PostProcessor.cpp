#include "eyelib/render/post/PostProcessor.h"
#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/core/Log.h"
#include <cmath>

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
    DestroyFramebuffers();
}

void PostProcessor::DestroyFramebuffers() {
    if (!m_deviceRHI) return;
    if (m_singleTexSet) { m_deviceRHI->DestroyDescriptorSet(m_singleTexSet); m_singleTexSet = nullptr; }
    if (m_singleTexLayout) { m_deviceRHI->DestroyPipelineLayout(m_singleTexLayout); m_singleTexLayout = nullptr; }
    for (auto& rt : m_pingPongRT) {
        if (rt) {
            m_deviceRHI->DestroyRenderTarget(rt);
            rt = nullptr;
        }
    }
    if (m_outputRT) { m_deviceRHI->DestroyRenderTarget(m_outputRT); m_outputRT = nullptr; }
    if (m_bloomExtractRT) { m_deviceRHI->DestroyRenderTarget(m_bloomExtractRT); m_bloomExtractRT = nullptr; }
    if (m_ssaoRT) { m_deviceRHI->DestroyRenderTarget(m_ssaoRT); m_ssaoRT = nullptr; }
    if (m_ssaoBlurRT) { m_deviceRHI->DestroyRenderTarget(m_ssaoBlurRT); m_ssaoBlurRT = nullptr; }
}

bool PostProcessor::Initialize(uint32_t width, uint32_t height, rhi::IDevice* device) {
    m_width = width;
    m_height = height;
    m_lastOutputValid = false;
    if (device) m_deviceRHI = device;
    if (!m_deviceRHI) {
        core::Log::Error("PostProcessor::Initialize: no RHI device");
        return false;
    }
    core::Log::Info("PostProcessor::Initialize: loading shaders");
    LoadShaders();
    core::Log::Info("PostProcessor::Initialize: creating fullscreen quad");
    core::Log::Info("PostProcessor::Initialize: creating framebuffers");
    CreateFramebuffers();
    // Pipeline layout + descriptor set for single-texture passes (Phase 3)
    {
        rhi::PipelineLayoutDesc layoutDesc;
        layoutDesc.bindings = {{0u, rhi::BindingType::Texture}};
        m_singleTexLayout = m_deviceRHI->CreatePipelineLayout(layoutDesc);
        m_singleTexSet = m_singleTexLayout ? m_deviceRHI->CreateDescriptorSet(m_singleTexLayout) : nullptr;
    }
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
    m_ssaoBlurRT = m_deviceRHI->CreateRenderTarget(m_width, m_height, r8, rhi::RHITextureFormat::None, 1);
    if (!m_ssaoBlurRT) core::Log::Error("PostProcessor::CreateFramebuffers: ssaoBlurRT failed");
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

    m_ssaoBlurShader = std::make_shared<Shader>();
    if (!m_ssaoBlurShader->Load(m_deviceRHI, "passes/post/ssao_blur", fullscreenLayout)) {
        core::Log::WarnF("PostProcessor: Failed to load SSAO blur shader");
    }
}

void PostProcessor::ReloadShaders() {
    if (!m_deviceRHI) return;
    if (m_toneMappingShader) m_toneMappingShader->Reload();
    if (m_bloomExtractShader) m_bloomExtractShader->Reload();
    if (m_bloomBlurShader) m_bloomBlurShader->Reload();
    if (m_bloomCompositeShader) m_bloomCompositeShader->Reload();
    if (m_blurShader) m_blurShader->Reload();
    if (m_chromaticAberrationShader) m_chromaticAberrationShader->Reload();
    if (m_fxaaShader) m_fxaaShader->Reload();
    if (m_ssaoShader) m_ssaoShader->Reload();
    if (m_ssaoBlurShader) m_ssaoBlurShader->Reload();
    if (m_ssrShader) m_ssrShader->Reload();
    if (m_motionBlurShader) m_motionBlurShader->Reload();
    if (m_vignetteShader) m_vignetteShader->Reload();
    if (m_filmGrainShader) m_filmGrainShader->Reload();
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
            m_ssaoBlendShader->Bind(m_deviceRHI);
            m_ssaoBlendShader->SetInt("u_Scene", 0);
            m_ssaoBlendShader->SetInt("u_SSAO", 1);
            cmd->BindTexture(toneMappedTexture, 0);
            cmd->BindTexture(ssaoTexture, 1);
            if (m_ssaoBlendShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_ssaoBlendShader->GetPipelineRHI()); }
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
            m_bloomCompositeShader->Bind(m_deviceRHI);
            m_bloomCompositeShader->SetInt("u_SceneTexture", 0);
            m_bloomCompositeShader->SetInt("u_BloomTexture", 1);
            m_bloomCompositeShader->SetFloat("u_BloomIntensity", settings.bloomIntensity);
            cmd->BindTexture(toneMappedTexture, 0);
            cmd->BindTexture(bloomTexture, 1);
            if (m_bloomCompositeShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_bloomCompositeShader->GetPipelineRHI()); }
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

    m_toneMappingShader->Bind(m_deviceRHI);
    m_toneMappingShader->SetInt("u_HDRTexture", 0);
    m_toneMappingShader->SetFloat("u_Exposure", settings.exposure);
    m_toneMappingShader->SetBool("u_UseACES", settings.useACES);

    cmd->BindTexture(hdrTexture, 0);
    if (m_toneMappingShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_toneMappingShader->GetPipelineRHI()); }
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

    m_bloomExtractShader->Bind(m_deviceRHI);
    m_bloomExtractShader->SetInt("u_HDRTexture", 0);
    m_bloomExtractShader->SetFloat("u_Threshold", threshold);
    cmd->BindTexture(hdrTexture, 0);
    if (m_bloomExtractShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_bloomExtractShader->GetPipelineRHI()); }
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

    m_bloomBlurShader->Bind(m_deviceRHI);
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
        if (m_bloomBlurShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_bloomBlurShader->GetPipelineRHI()); }
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

    m_blurShader->Bind(m_deviceRHI);
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
        if (m_blurShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_blurShader->GetPipelineRHI()); }
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
    m_chromaticAberrationShader->Bind(m_deviceRHI);
    m_chromaticAberrationShader->SetInt("u_Texture", 0);
    m_chromaticAberrationShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_chromaticAberrationShader->SetFloat("u_Intensity", intensity);
    cmd->BindTexture(texture, 0);
    if (m_chromaticAberrationShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_chromaticAberrationShader->GetPipelineRHI()); }
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
    m_fxaaShader->Bind(m_deviceRHI);
    m_fxaaShader->SetInt("u_Texture", 0);
    m_fxaaShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    if (m_singleTexSet) {
        m_singleTexSet->UpdateTexture(0, texture);
        cmd->BindDescriptorSet(m_singleTexSet);
    } else {
        cmd->BindTexture(texture, 0);
    }
    if (m_fxaaShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_fxaaShader->GetPipelineRHI()); }
    m_fxaaShader->Unbind();
    return GetOutputTextureRHI();
}

namespace {
constexpr int kSSAOBlurKernelSize = 16;
constexpr int kSSAOBlurSampleCount = 6;
constexpr float kSSAOBlurStdDev = 1.0f;

void BuildGaussianKernel(float* outKernel, int maxSamples, int gaussianHalfWidth, float stdDev) {
    int count = (std::min)(maxSamples, (gaussianHalfWidth + 1) / 2);
    for (int i = 0; i < count; ++i) {
        float x = static_cast<float>(i);
        outKernel[i] = std::exp(-(x * x) / (2.0f * stdDev * stdDev));
    }
}
} // namespace

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

    m_ssaoShader->Bind(m_deviceRHI);
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
    if (m_ssaoShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_ssaoShader->GetPipelineRHI()); }
    m_ssaoShader->Unbind();

    // Two-pass bilateral blur (Limitless-style) to reduce SSAO noise
    rhi::ITexture* depthTex = gbuffer.GetDepthTextureRHI();
    if (m_ssaoBlurShader && m_ssaoBlurShader->IsValid() && m_ssaoBlurRT && depthTex) {
        float kernel[kSSAOBlurKernelSize];
        BuildGaussianKernel(kernel, kSSAOBlurKernelSize, 11, kSSAOBlurStdDev);
        const float farOverEdge = 100.0f / 0.0625f;
        const math::Vec2 res(static_cast<float>(m_width), static_cast<float>(m_height));

        for (int pass = 0; pass < 2; ++pass) {
            rhi::IRenderTarget* dstRT = (pass == 0) ? m_ssaoBlurRT : m_ssaoRT;
            rhi::ITexture* srcTex = (pass == 0) ? GetColorTexture(m_ssaoRT) : GetColorTexture(m_ssaoBlurRT);
            rhi::ClearAndSetRenderTargetWithViewport(cmd, dstRT, 0, 0, m_width, m_height, cv);
            cmd->SetDepthTest(false);
            cmd->SetDepthWrite(false);
            cmd->SetCullFace(false);
            cmd->SetBlend(false);

            m_ssaoBlurShader->Bind(m_deviceRHI);
            m_ssaoBlurShader->SetInt("u_SSAO", 0);
            m_ssaoBlurShader->SetInt("u_Depth", 1);
            m_ssaoBlurShader->SetVec2("u_Axis", (pass == 0) ? math::Vec2(1.0f, 0.0f) : math::Vec2(0.0f, 1.0f));
            m_ssaoBlurShader->SetVec2("u_Resolution", res);
            m_ssaoBlurShader->SetFloat("u_NearPlane", 0.1f);
            m_ssaoBlurShader->SetFloat("u_FarPlane", 1000.0f);
            m_ssaoBlurShader->SetFloat("u_FarPlaneOverEdgeDistance", farOverEdge);
            m_ssaoBlurShader->SetInt("u_SampleCount", kSSAOBlurSampleCount);
            for (int i = 0; i < kSSAOBlurSampleCount; ++i) {
                m_ssaoBlurShader->SetFloat(std::string("u_Kernel[") + std::to_string(i) + "]", kernel[i]);
            }
            cmd->BindTexture(srcTex, 0);
            cmd->BindTexture(depthTex, 1);
            if (m_ssaoBlurShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_ssaoBlurShader->GetPipelineRHI()); }
            m_ssaoBlurShader->Unbind();
        }
    }

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

    m_ssrShader->Bind(m_deviceRHI);
    m_ssrShader->SetInt("g_Position", 0);
    m_ssrShader->SetInt("g_Normal", 1);
    m_ssrShader->SetInt("g_Albedo", 2);
    m_ssrShader->SetInt("g_PBRParams", 3);
    m_ssrShader->SetInt("g_Depth", 4);
    m_ssrShader->SetInt("u_SceneTexture", 5);
    m_ssrShader->SetInt("u_Skybox", 6);
    m_ssrShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_ssrShader->SetFloat("u_StepSize", settings.ssrStepSize);
    m_ssrShader->SetInt("u_MaxSteps", settings.ssrMaxSteps);
    m_ssrShader->SetFloat("u_Thickness", settings.ssrThickness);
    m_ssrShader->SetFloat("u_NearPlane", settings.ssrNearPlane);
    m_ssrShader->SetFloat("u_FarPlane", settings.ssrFarPlane);
    m_ssrShader->SetMat4("u_ViewMatrix", viewMatrix);
    m_ssrShader->SetMat4("u_ProjectionMatrix", projectionMatrix);
    m_ssrShader->SetMat4("u_InverseViewMatrix", invViewMatrix);
    m_ssrShader->SetMat4("u_InverseProjectionMatrix", invProjectionMatrix);
    m_ssrShader->SetVec3("u_ViewPos", viewPos);
    cmd->BindTexture(gbuffer.GetPositionTextureRHI(), 0);
    cmd->BindTexture(gbuffer.GetNormalTextureRHI(), 1);
    cmd->BindTexture(gbuffer.GetAlbedoTextureRHI(), 2);
    if (rhi::ITexture* pbrTex = gbuffer.GetPBRParamsTextureRHI())
        cmd->BindTexture(pbrTex, 3);
    rhi::ITexture* depthTex = gbuffer.GetDepthTextureRHI();
    if (depthTex) cmd->BindTexture(depthTex, 4);
    cmd->BindTexture(sceneTexture, 5);
    if (skyboxTexture) cmd->BindTexture(skyboxTexture, 6);
    if (m_ssrShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_ssrShader->GetPipelineRHI()); }
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

    m_motionBlurShader->Bind(m_deviceRHI);
    m_motionBlurShader->SetInt("u_Texture", 0);
    m_motionBlurShader->SetInt("g_Velocity", 1);
    m_motionBlurShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_motionBlurShader->SetFloat("u_Intensity", settings.motionBlurIntensity);
    m_motionBlurShader->SetInt("u_Samples", settings.motionBlurSamples);
    cmd->BindTexture(texture, 0);
    cmd->BindTexture(velocityTexture, 1);
    if (m_motionBlurShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_motionBlurShader->GetPipelineRHI()); }
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
    m_vignetteShader->Bind(m_deviceRHI);
    m_vignetteShader->SetInt("u_Texture", 0);
    m_vignetteShader->SetFloat("u_Intensity", settings.vignetteIntensity);
    m_vignetteShader->SetFloat("u_Radius", settings.vignetteRadius);
    m_vignetteShader->SetFloat("u_Softness", settings.vignetteSoftness);
    cmd->BindTexture(texture, 0);
    if (m_vignetteShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_vignetteShader->GetPipelineRHI()); }
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
    m_filmGrainShader->Bind(m_deviceRHI);
    m_filmGrainShader->SetInt("u_Texture", 0);
    m_filmGrainShader->SetFloat("u_Intensity", settings.filmGrainIntensity);
    m_filmGrainShader->SetFloat("u_Time", time);
    cmd->BindTexture(texture, 0);
    if (m_filmGrainShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_filmGrainShader->GetPipelineRHI()); }
    m_filmGrainShader->Unbind();
    return GetOutputTextureRHI();
}

void PostProcessor::AddPostPassesToFrameGraph(FrameGraph* fg, int hdrInput, int toneMappedOutput, int finalOutput,
                                              const Settings& settings, rhi::ICommandList* cmd) {
    if (!fg || !cmd || !m_pingPongRT[0] || !m_outputRT) return;

    fg->AddPass("Post.ToneMap",
        [this, hdrInput, settings, cmd](const FGPassResources& res) {
            rhi::ITexture* hdrTex = res.getTextureRHI(hdrInput);
            if (!hdrTex || !m_toneMappingShader || !m_toneMappingShader->IsValid()) return;
            cmd->SetRenderTarget(m_pingPongRT[0]);
            cmd->SetViewport(0, 0, res.width, res.height);
            cmd->SetDepthTest(false);
            cmd->SetDepthWrite(false);
            cmd->SetCullFace(false);
            cmd->SetBlend(false);
            rhi::ClearValue clearBlack = {0.f, 0.f, 0.f, 1.f};
            cmd->ClearRenderTarget(m_pingPongRT[0], clearBlack);
            m_toneMappingShader->Bind(m_deviceRHI);
            m_toneMappingShader->SetInt("u_HDRTexture", 0);
            m_toneMappingShader->SetFloat("u_Exposure", settings.exposure);
            m_toneMappingShader->SetBool("u_UseACES", settings.useACES);
            cmd->BindTexture(hdrTex, 0);
            if (m_toneMappingShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_toneMappingShader->GetPipelineRHI()); }
            m_toneMappingShader->Unbind();
            m_lastOutputValid = true;
        },
        { hdrInput }, { toneMappedOutput }, true);

    fg->AddPass("Post.FXAA",
        [this, toneMappedOutput, settings, cmd](const FGPassResources& res) {
            rhi::ITexture* toneMappedTex = res.getTextureRHI(toneMappedOutput);
            if (!toneMappedTex || !m_fxaaShader || !m_fxaaShader->IsValid()) return;
            cmd->SetRenderTarget(m_outputRT);
            cmd->SetViewport(0, 0, res.width, res.height);
            cmd->SetDepthTest(false);
            cmd->SetDepthWrite(false);
            cmd->SetCullFace(false);
            cmd->SetBlend(false);
            m_fxaaShader->Bind(m_deviceRHI);
            m_fxaaShader->SetInt("u_Texture", 0);
            m_fxaaShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(res.width), static_cast<float>(res.height)));
            cmd->BindTexture(toneMappedTex, 0);
            if (m_fxaaShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_fxaaShader->GetPipelineRHI()); }
            m_fxaaShader->Unbind();
        },
        { toneMappedOutput }, { finalOutput }, true);
}

rhi::ITexture* PostProcessor::GetColorTexture(rhi::IRenderTarget* rt) const {
    return rt ? rt->GetColorTexture(0) : nullptr;
}

rhi::ITexture* PostProcessor::GetOutputTextureRHI() const {
    return GetColorTexture(m_outputRT);
}

} // namespace render
} // namespace eyelib
