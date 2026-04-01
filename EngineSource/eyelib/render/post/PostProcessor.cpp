#include "eyelib/render/post/PostProcessor.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/render/rhi/PipelineAttachmentFormatSets.h"
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/core/Log.h"
#include <cmath>
#include <cstring>
#include <optional>

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
    m_lastOutputTexture = nullptr;
    m_lastOutputRT = nullptr;
    for (auto& frameSets : m_transientPostSetsByFrame) {
        frameSets.clear();
    }
    m_transientPostFrameIndex = 0xFFFFFFFFu;
    if (m_singleTexSet) { m_deviceRHI->DestroyDescriptorSet(m_singleTexSet); m_singleTexSet = nullptr; }
    if (m_singleTexLayout) { m_deviceRHI->DestroyPipelineLayout(m_singleTexLayout); m_singleTexLayout = nullptr; }
    if (m_postPassSet) { m_deviceRHI->DestroyDescriptorSet(m_postPassSet); m_postPassSet = nullptr; }
    m_postPassLayout = nullptr;
    if (m_frameSet) { m_deviceRHI->DestroyDescriptorSet(m_frameSet); m_frameSet = nullptr; }
    if (m_frameUBO) { m_deviceRHI->DestroyBuffer(m_frameUBO); m_frameUBO = nullptr; }
    for (auto& rt : m_pingPongRT) {
        if (rt) {
            m_deviceRHI->DestroyRenderTarget(rt);
            rt = nullptr;
        }
    }
    if (m_outputRT) { m_deviceRHI->DestroyRenderTarget(m_outputRT); m_outputRT = nullptr; }
    if (m_ldrPingPong) { m_deviceRHI->DestroyRenderTarget(m_ldrPingPong); m_ldrPingPong = nullptr; }
    if (m_bloomExtractRT) { m_deviceRHI->DestroyRenderTarget(m_bloomExtractRT); m_bloomExtractRT = nullptr; }
    if (m_bloomBlurScratchRT) { m_deviceRHI->DestroyRenderTarget(m_bloomBlurScratchRT); m_bloomBlurScratchRT = nullptr; }
    if (m_ssaoRT) { m_deviceRHI->DestroyRenderTarget(m_ssaoRT); m_ssaoRT = nullptr; }
    if (m_ssaoBlurRT) { m_deviceRHI->DestroyRenderTarget(m_ssaoBlurRT); m_ssaoBlurRT = nullptr; }
    if (m_ssrPrevRT) { m_deviceRHI->DestroyRenderTarget(m_ssrPrevRT); m_ssrPrevRT = nullptr; }
    m_ssrHasPrevFrame = false;
    for (rhi::IRenderTarget* rt : m_depthPyramidRTs) {
        if (rt) { m_deviceRHI->DestroyRenderTarget(rt); }
    }
    m_depthPyramidRTs.clear();
    if (m_depthPyramidTexture) { m_deviceRHI->DestroyTexture(m_depthPyramidTexture); m_depthPyramidTexture = nullptr; }
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
    m_useVulkanTransientPostSets = (std::strcmp(m_deviceRHI->GetShaderBackendName(), "vulkan") == 0);
    LoadShaders();
    CreateFramebuffers();
    // Pipeline layout + descriptor set for single-texture passes (Phase 3)
    {
        rhi::PipelineLayoutDesc layoutDesc;
        layoutDesc.bindings = {{0u, rhi::BindingType::Texture}};
        m_singleTexLayout = m_deviceRHI->CreatePipelineLayout(layoutDesc);
        m_singleTexSet = m_singleTexLayout ? m_deviceRHI->CreateDescriptorSet(m_singleTexLayout) : nullptr;
    }
    // Resource-space Post path: Frame (set 0) + PostPass (set 1) for Vulkan
    rhi::IPipelineLayout* frameLayout = m_deviceRHI->GetResourceSetLayout(rhi::IDevice::ResourceSetType::Frame);
    m_postPassLayout = m_deviceRHI->GetResourceSetLayout(rhi::IDevice::ResourceSetType::PostPass);
    if (frameLayout && m_postPassLayout) {
        m_frameSet = m_deviceRHI->CreateDescriptorSet(frameLayout);
        if (m_frameSet) {
            rhi::BufferDesc uboDesc;
            uboDesc.sizeInBytes = 256;
            uboDesc.usage = rhi::BufferUsage::Uniform;
            uboDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_frameUBO = m_deviceRHI->CreateBuffer(uboDesc, nullptr);
            if (m_frameUBO) {
                m_frameSet->UpdateUniformBuffer(0, m_frameUBO, 0, 128);
                m_frameSet->UpdateUniformBuffer(1, m_frameUBO, 128, 128);
            }
        }
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

void PostProcessor::RecycleTransientPostSetsIfNeeded() {
    if (!m_useVulkanTransientPostSets || !m_deviceRHI) return;
    auto* vkDevice = static_cast<rhi::DeviceVK*>(m_deviceRHI);
    const uint32_t frameIndex = vkDevice->GetCurrentFrameIndex() % kMaxFramesInFlight;
    if (frameIndex == m_transientPostFrameIndex) return;
    // Descriptor allocator/pool reset owns transient set lifetime; only clear CPU tracking here.
    m_transientPostSetsByFrame[frameIndex].clear();
    m_transientPostFrameIndex = frameIndex;
}

void PostProcessor::BindPostPassDescriptorSets(rhi::ICommandList* cmd, rhi::IDescriptorSet* postPassSet) {
    if (!cmd || !postPassSet) return;
    if (m_frameSet) {
        rhi::IDescriptorSet* sets[2] = {m_frameSet, postPassSet};
        cmd->BindDescriptorSets(0, 2, sets);
    } else {
        cmd->BindDescriptorSets(1, 1, &postPassSet);
    }
}

rhi::IDescriptorSet* PostProcessor::CreateTransientPostSet(const std::vector<std::pair<uint32_t, rhi::ITexture*>>& bindings) {
    if (!m_useVulkanTransientPostSets || !m_deviceRHI) return nullptr;
    if (!m_postPassLayout)
        m_postPassLayout = m_deviceRHI->GetResourceSetLayout(rhi::IDevice::ResourceSetType::PostPass);
    if (!m_postPassLayout) return nullptr;
    RecycleTransientPostSetsIfNeeded();
    rhi::IDescriptorSet* set = m_deviceRHI->CreateDescriptorSet(m_postPassLayout);
    if (!set) return nullptr;
    for (const auto& b : bindings) {
        set->UpdateTexture(b.first, b.second);
    }
    m_transientPostSetsByFrame[m_transientPostFrameIndex].push_back(set);
    return set;
}

void PostProcessor::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    if (width == 0 || height == 0) {
        m_width = width;
        m_height = height;
        m_lastOutputValid = false;
        m_lastOutputTexture = nullptr;
        m_lastOutputRT = nullptr;
        return;
    }
    m_width = width;
    m_height = height;
    m_lastOutputValid = false;
    m_lastOutputTexture = nullptr;
    m_lastOutputRT = nullptr;
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
    m_bloomBlurScratchRT = m_deviceRHI->CreateRenderTarget(m_width, m_height, rgb16f, rhi::RHITextureFormat::None, 1);
    if (!m_bloomBlurScratchRT) core::Log::Error("PostProcessor::CreateFramebuffers: bloomBlurScratchRT failed");
    m_outputRT = m_deviceRHI->CreateRenderTarget(m_width, m_height, rgba8, rhi::RHITextureFormat::None, 1);
    if (!m_outputRT) core::Log::Error("PostProcessor::CreateFramebuffers: outputRT failed");
    m_ldrPingPong = m_deviceRHI->CreateRenderTarget(m_width, m_height, rgba8, rhi::RHITextureFormat::None, 1);
    if (!m_ldrPingPong) core::Log::Error("PostProcessor::CreateFramebuffers: ldrPingPong failed");
    m_ssaoRT = m_deviceRHI->CreateRenderTarget(m_width, m_height, r8, rhi::RHITextureFormat::None, 1);
    if (!m_ssaoRT) core::Log::Error("PostProcessor::CreateFramebuffers: ssaoRT failed");
    m_ssaoBlurRT = m_deviceRHI->CreateRenderTarget(m_width, m_height, r8, rhi::RHITextureFormat::None, 1);
    if (!m_ssaoBlurRT) core::Log::Error("PostProcessor::CreateFramebuffers: ssaoBlurRT failed");

    m_ssrPrevRT = m_deviceRHI->CreateRenderTarget(m_width, m_height, rgb16f, rhi::RHITextureFormat::None, 1);
    if (!m_ssrPrevRT) core::Log::Error("PostProcessor::CreateFramebuffers: ssrPrevRT failed");
    m_ssrHasPrevFrame = false;

    // Hi-Z depth pyramid for SSR (RGBA16F, R = linear view-space depth)
    rhi::TextureDesc pyramidDesc;
    pyramidDesc.type = rhi::TextureType::Texture2D;
    pyramidDesc.width = m_width;
    pyramidDesc.height = m_height;
    pyramidDesc.format = rhi::RHITextureFormat::RGBA16F;
    pyramidDesc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::RenderTarget;
    pyramidDesc.mipLevels = kDepthPyramidLevels;
    m_depthPyramidTexture = m_deviceRHI->CreateTexture(pyramidDesc, nullptr);
    m_depthPyramidRTs.clear();
    if (m_depthPyramidTexture) {
        for (uint32_t mip = 0; mip < kDepthPyramidLevels; ++mip) {
            rhi::IRenderTarget* rt = m_deviceRHI->CreateRenderTargetFromTexture(m_depthPyramidTexture, 0, mip);
            m_depthPyramidRTs.push_back(rt);
        }
    }
}

void PostProcessor::LoadShaders() {
    if (!m_deviceRHI) return;
    const rhi::VertexLayout fullscreenLayout = GetFullscreenVertexLayout();
    const rhi::PipelineLayoutKind postLayout = rhi::PipelineLayoutKind::Post;

    m_toneMappingShader = std::make_shared<Shader>();
    if (!m_toneMappingShader->Load(m_deviceRHI, "passes/post/tonemap", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                                   std::optional{rhi::StandardAttachmentFormats::PostLDR8()})) {
        core::Log::WarnF("PostProcessor: Failed to load tone mapping shader");
    }

    m_bloomExtractShader = std::make_shared<Shader>();
    if (!m_bloomExtractShader->Load(m_deviceRHI, "passes/post/bloom_extract", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                                    std::optional{rhi::StandardAttachmentFormats::PostHDR()})) {
        core::Log::WarnF("PostProcessor: Failed to load bloom extract shader");
    }

    m_bloomBlurShader = std::make_shared<Shader>();
    if (!m_bloomBlurShader->Load(m_deviceRHI, "passes/post/bloom_blur", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                                 std::optional{rhi::StandardAttachmentFormats::PostHDR()})) {
        core::Log::WarnF("PostProcessor: Failed to load bloom blur shader");
    }

    m_bloomCompositeShader = std::make_shared<Shader>();
    if (!m_bloomCompositeShader->Load(m_deviceRHI, "passes/post/bloom_composite", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                                      std::optional{rhi::StandardAttachmentFormats::PostHDR()})) {
        core::Log::WarnF("PostProcessor: Failed to load bloom composite shader");
    }

    m_blurShader = std::make_shared<Shader>();
    if (!m_blurShader->Load(m_deviceRHI, "passes/post/bloom_blur", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                            std::optional{rhi::StandardAttachmentFormats::PostHDR()})) {
        core::Log::WarnF("PostProcessor: Failed to load blur shader");
    }

    m_chromaticAberrationShader = std::make_shared<Shader>();
    if (!m_chromaticAberrationShader->Load(m_deviceRHI, "passes/post/chromatic_aberration", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                                           std::optional{rhi::StandardAttachmentFormats::PostLDR8()})) {
        core::Log::WarnF("PostProcessor: Failed to load chromatic aberration shader");
    }

    m_fxaaShader = std::make_shared<Shader>();
    if (!m_fxaaShader->Load(m_deviceRHI, "passes/post/fxaa", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                            std::optional{rhi::StandardAttachmentFormats::PostLDR8()})) {
        core::Log::WarnF("PostProcessor: Failed to load FXAA shader");
    }

    m_ssaoShader = std::make_shared<Shader>();
    if (!m_ssaoShader->Load(m_deviceRHI, "passes/post/ssao", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                            std::optional{rhi::StandardAttachmentFormats::PostR8()})) {
        core::Log::WarnF("PostProcessor: Failed to load SSAO shader");
    }

    m_ssrShader = std::make_shared<Shader>();
    if (!m_ssrShader->Load(m_deviceRHI, "passes/post/ssr", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                           std::optional{rhi::StandardAttachmentFormats::PostHDR()})) {
        core::Log::WarnF("PostProcessor: Failed to load SSR shader");
    }

    m_motionBlurShader = std::make_shared<Shader>();
    if (!m_motionBlurShader->Load(m_deviceRHI, "passes/post/motion_blur", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                                  std::optional{rhi::StandardAttachmentFormats::PostLDR8()})) {
        core::Log::WarnF("PostProcessor: Failed to load motion blur shader");
    }

    m_vignetteShader = std::make_shared<Shader>();
    if (!m_vignetteShader->Load(m_deviceRHI, "passes/post/vignette", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                                std::optional{rhi::StandardAttachmentFormats::PostLDR8()})) {
        core::Log::WarnF("PostProcessor: Failed to load vignette shader");
    }

    m_filmGrainShader = std::make_shared<Shader>();
    if (!m_filmGrainShader->Load(m_deviceRHI, "passes/post/film_grain", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                                 std::optional{rhi::StandardAttachmentFormats::PostLDR8()})) {
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
    m_ssaoBlendShader->BuildFromSource(m_deviceRHI, ssaoBlendVert, ssaoBlendFrag, fullscreenLayout, rhi::PrimitiveTopology::TriangleList,
                                       rhi::PipelineLayoutKind::PushOnly, std::optional{rhi::StandardAttachmentFormats::PostHDR()});

    m_ssaoBlurShader = std::make_shared<Shader>();
    if (!m_ssaoBlurShader->Load(m_deviceRHI, "passes/post/ssao_blur", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                                std::optional{rhi::StandardAttachmentFormats::PostR8()})) {
        core::Log::WarnF("PostProcessor: Failed to load SSAO blur shader");
    }
    m_copyShader = std::make_shared<Shader>();
    if (!m_copyShader->Load(m_deviceRHI, "passes/post/copy", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                            std::optional{rhi::StandardAttachmentFormats::PostCopyBGRA()})) {
        core::Log::WarnF("PostProcessor: Failed to load copy shader");
    }
    m_displayEncodeShader = std::make_shared<Shader>();
    if (!m_displayEncodeShader->Load(m_deviceRHI, "passes/post/display_encode", fullscreenLayout, rhi::PrimitiveTopology::TriangleList,
                                     postLayout, std::optional{rhi::StandardAttachmentFormats::PostLDR8()})) {
        core::Log::WarnF("PostProcessor: Failed to load display_encode shader");
    }
    m_depthPyramidShader = std::make_shared<Shader>();
    if (!m_depthPyramidShader->Load(m_deviceRHI, "passes/post/depth_pyramid", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, postLayout,
                                    std::optional{rhi::StandardAttachmentFormats::PostHDR()})) {
        core::Log::WarnF("PostProcessor: Failed to load depth_pyramid shader");
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
                                                  const math::Vec3& viewPos, rhi::ITexture* skyboxTexture, float time,
                                                  rhi::ITexture* reflectionProbeTexture) {
    if (!m_postPassLayout && m_deviceRHI)
        m_postPassLayout = m_deviceRHI->GetResourceSetLayout(rhi::IDevice::ResourceSetType::PostPass);
    m_lastOutputValid = false;
    m_lastOutputTexture = nullptr;
    m_lastOutputRT = nullptr;
    if (!hdrInputTexture || !GetOutputTextureRHI()) {
        return hdrInputTexture;
    }
    rhi::ICommandList* cmdPost = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    if (cmdPost)
        rhi::BarrierTextureForShaderRead(cmdPost, hdrInputTexture);

    // SSAO factor texture (computed in linear/view space)
    rhi::ITexture* ssaoTexture = nullptr;
    if (settings.enableSSAO) {
        ssaoTexture = ApplySSAO(gbuffer, settings, viewMatrix, projectionMatrix);
    }

    // SSR on HDR lit buffer (before SSAO/bloom/tonemap)
    rhi::ITexture* preToneMapTexture = hdrInputTexture;
    if (settings.enableSSR && GetPingPongRT(0)) {
        preToneMapTexture = ApplySSR(hdrInputTexture, gbuffer, skyboxTexture, settings,
                                      viewMatrix, projectionMatrix, invViewMatrix, invProjectionMatrix, viewPos,
                                      GetPingPongRT(0), reflectionProbeTexture);
        if (cmdPost && preToneMapTexture)
            rhi::BarrierTextureForShaderRead(cmdPost, preToneMapTexture);
    }

    // Linear HDR working buffer: SSR → (optional) SSAO multiply → (optional) bloom add → tonemap
    rhi::ITexture* hdrWorking = preToneMapTexture;

    if (settings.enableSSAO && ssaoTexture && m_ssaoBlendShader && m_ssaoBlendShader->IsValid()) {
        hdrWorking = ApplySSAOBlendHDR(hdrWorking, ssaoTexture);
        if (cmdPost && hdrWorking)
            rhi::BarrierTextureForShaderRead(cmdPost, hdrWorking);
    }

    rhi::ITexture* bloomTexture = nullptr;
    if (settings.enableBloom) {
        bloomTexture = ExtractBloom(hdrWorking, settings.bloomThreshold);
        if (bloomTexture) {
            bloomTexture = BlurTexture(bloomTexture, settings.bloomIterations);
            if (bloomTexture && m_bloomCompositeShader && m_bloomCompositeShader->IsValid()) {
                hdrWorking = ApplyBloomCompositeHDR(hdrWorking, bloomTexture, settings);
                if (cmdPost && hdrWorking)
                    rhi::BarrierTextureForShaderRead(cmdPost, hdrWorking);
            }
        }
    }

    rhi::ITexture* toneMappedTexture = ApplyToneMapping(hdrWorking, settings);

    // General Blur (if enabled, applied after bloom)
    if (settings.enableBlur) {
        toneMappedTexture = ApplyGeneralBlur(toneMappedTexture, settings.blurIntensity, settings.blurIterations);
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

    // FXAA (on linear LDR, before display encoding)
    if (settings.enableFXAA) {
        toneMappedTexture = ApplyFXAA(toneMappedTexture);
    }

    // sRGB OETF once: UNORM8 targets and swapchain blit expect gamma-encoded values for correct display.
    toneMappedTexture = ApplyDisplayEncoding(toneMappedTexture);

    m_lastOutputTexture = toneMappedTexture;
    m_lastOutputRT = ResolveRenderTargetForTexture(toneMappedTexture);
    return toneMappedTexture;
}

 rhi::ITexture* PostProcessor::ApplyToneMapping(rhi::ITexture* hdrTexture, const Settings& settings) {
    if (!m_toneMappingShader || !m_toneMappingShader->IsValid()) {
        return hdrTexture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    rhi::IRenderTarget* targetRT = SelectPostTargetLDR(hdrTexture);
    if (!cmd || !targetRT) return hdrTexture;
    rhi::BarrierTextureForShaderRead(cmd, hdrTexture);
    cmd->SetRenderTarget(targetRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);

    rhi::ClearValue clearBlack = {0.f, 0.f, 0.f, 1.f};
    cmd->ClearRenderTarget(targetRT, clearBlack);

    m_toneMappingShader->Bind(m_deviceRHI);
    if (m_postPassLayout) {
        rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, hdrTexture}});
        abi::WriteTonemapPushConstants(cmd, settings.exposure, settings.useACES ? 1 : 0);
        if (postSet) {
            if (rhi::IPipeline* pipe = m_toneMappingShader->GetPipelineRHI())
                cmd->BindPipeline(pipe);
            BindPostPassDescriptorSets(cmd, postSet);
        } else {
            m_toneMappingShader->Unbind();
            return hdrTexture;
        }
    } else {
        m_toneMappingShader->SetInt("u_HDRTexture", 0);
        m_toneMappingShader->SetFloat("u_Exposure", settings.exposure);
        m_toneMappingShader->SetBool("u_UseACES", settings.useACES);
        cmd->BindTexture(hdrTexture, 0);
    }
    if (m_toneMappingShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_toneMappingShader->GetPipelineRHI()); }
    m_lastOutputValid = true;
    m_lastOutputTexture = GetColorTexture(targetRT);
    m_lastOutputRT = targetRT;
    m_toneMappingShader->Unbind();
    return m_lastOutputTexture;
}

 rhi::ITexture* PostProcessor::ExtractBloom(rhi::ITexture* hdrTexture, float threshold) {
    if (!m_bloomExtractShader || !m_bloomExtractShader->IsValid()) {
        return nullptr;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
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
    if (m_postPassLayout) {
        rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, hdrTexture}});
        if (postSet) {
            if (rhi::IPipeline* pipe = m_bloomExtractShader->GetPipelineRHI())
                cmd->BindPipeline(pipe);
            BindPostPassDescriptorSets(cmd, postSet);
            abi::WriteBloomExtractPushConstant(cmd, threshold);
        } else {
            m_bloomExtractShader->Unbind();
            return nullptr;
        }
    } else {
        cmd->BindTexture(hdrTexture, 0);
    }
    if (m_bloomExtractShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_bloomExtractShader->GetPipelineRHI()); }
    m_bloomExtractShader->Unbind();
    rhi::BarrierTextureForShaderRead(cmd, GetColorTexture(m_bloomExtractRT));
    return GetColorTexture(m_bloomExtractRT);
}

 rhi::ITexture* PostProcessor::BlurTexture(rhi::ITexture* texture, int iterations) {
    if (!m_bloomBlurShader || !m_bloomBlurShader->IsValid()) {
        return texture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    if (!cmd) return texture;
    if (!m_bloomExtractRT || !m_bloomBlurScratchRT) return texture;
    bool horizontal = true;
    rhi::ITexture* currentTexture = texture;

    m_bloomBlurShader->Bind(m_deviceRHI);
    m_bloomBlurShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_bloomBlurShader->SetFloat("u_Intensity", 1.0f);

    for (int i = 0; i < iterations; ++i) {
        // Ping-pong only between bloom extract and dedicated scratch so we never overwrite HDR scene in m_pingPongRT.
        rhi::IRenderTarget* rt = (i % 2 == 0) ? m_bloomBlurScratchRT : m_bloomExtractRT;
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
        if (m_postPassLayout) {
            rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, currentTexture}});
            if (postSet) {
                if (rhi::IPipeline* pipe = m_bloomBlurShader->GetPipelineRHI())
                    cmd->BindPipeline(pipe);
                BindPostPassDescriptorSets(cmd, postSet);
                abi::WriteBloomBlurPushConstants(cmd, static_cast<float>(m_width), static_cast<float>(m_height), horizontal ? 1 : 0,
                                                 1.0f);
            } else {
                break;
            }
        } else {
            cmd->BindTexture(currentTexture, 0);
        }
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
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    if (!cmd) return texture;
    bool horizontal = true;
    rhi::ITexture* currentTexture = texture;

    m_blurShader->Bind(m_deviceRHI);
    m_blurShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_blurShader->SetFloat("u_Intensity", intensity);

    // Same HDR ping-pong as BlurTexture / bloom: pipeline color format is RGBA16F (see PipelineVK SelectRenderingFormats).
    for (int i = 0; i < iterations; ++i) {
        rhi::IRenderTarget* rt = m_pingPongRT[horizontal ? 0 : 1];
        if (!rt) break;
        rhi::BarrierTextureForShaderRead(cmd, currentTexture);
        cmd->SetRenderTarget(rt);
        cmd->SetViewport(0, 0, m_width, m_height);
        cmd->SetDepthTest(false);
        cmd->SetDepthWrite(false);
        cmd->SetCullFace(false);
        cmd->SetBlend(false);
        m_blurShader->SetBool("u_Horizontal", horizontal);
        m_blurShader->SetInt("u_Texture", 0);
        if (m_postPassLayout) {
            rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, currentTexture}});
            if (postSet) {
                if (rhi::IPipeline* pipe = m_blurShader->GetPipelineRHI())
                    cmd->BindPipeline(pipe);
                BindPostPassDescriptorSets(cmd, postSet);
                abi::WriteBloomBlurPushConstants(cmd, static_cast<float>(m_width), static_cast<float>(m_height), horizontal ? 1 : 0,
                                                 intensity);
            } else {
                break;
            }
        } else {
            cmd->BindTexture(currentTexture, 0);
        }
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
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    rhi::IRenderTarget* targetRT = SelectPostTargetLDR(texture);
    if (!cmd || !targetRT) return texture;
    rhi::BarrierTextureForShaderRead(cmd, texture);
    cmd->SetRenderTarget(targetRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);
    m_chromaticAberrationShader->Bind(m_deviceRHI);
    m_chromaticAberrationShader->SetInt("u_Texture", 0);
    m_chromaticAberrationShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    m_chromaticAberrationShader->SetFloat("u_Intensity", intensity);
    if (m_postPassLayout) {
        rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, texture}});
        if (postSet) {
            if (rhi::IPipeline* pipe = m_chromaticAberrationShader->GetPipelineRHI())
                cmd->BindPipeline(pipe);
            BindPostPassDescriptorSets(cmd, postSet);
            abi::WriteChromaticAberrationPushConstants(cmd, static_cast<float>(m_width), static_cast<float>(m_height), intensity);
        } else {
            m_chromaticAberrationShader->Unbind();
            return texture;
        }
    } else {
        cmd->BindTexture(texture, 0);
    }
    if (m_chromaticAberrationShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_chromaticAberrationShader->GetPipelineRHI()); }
    m_chromaticAberrationShader->Unbind();
    m_lastOutputTexture = GetColorTexture(targetRT);
    m_lastOutputRT = targetRT;
    return m_lastOutputTexture;
}

rhi::ITexture* PostProcessor::ApplyDisplayEncoding(rhi::ITexture* linearLdr) {
    if (!m_displayEncodeShader || !m_displayEncodeShader->IsValid()) {
        return linearLdr;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    rhi::IRenderTarget* targetRT = SelectPostTargetLDR(linearLdr);
    if (!cmd || !targetRT) return linearLdr;
    rhi::BarrierTextureForShaderRead(cmd, linearLdr);
    cmd->SetRenderTarget(targetRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);

    m_displayEncodeShader->Bind(m_deviceRHI);
    m_displayEncodeShader->SetInt("u_Texture", 0);
    if (m_postPassLayout) {
        rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, linearLdr}});
        if (postSet) {
            if (rhi::IPipeline* pipe = m_displayEncodeShader->GetPipelineRHI())
                cmd->BindPipeline(pipe);
            BindPostPassDescriptorSets(cmd, postSet);
        } else {
            m_displayEncodeShader->Unbind();
            return linearLdr;
        }
    } else {
        cmd->BindTexture(linearLdr, 0);
    }
    if (m_displayEncodeShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_displayEncodeShader->GetPipelineRHI()); }
    m_displayEncodeShader->Unbind();
    m_lastOutputTexture = GetColorTexture(targetRT);
    m_lastOutputRT = targetRT;
    return m_lastOutputTexture;
}

rhi::ITexture* PostProcessor::ApplySSAOBlendHDR(rhi::ITexture* hdrScene, rhi::ITexture* ssaoTexture) {
    if (!hdrScene || !ssaoTexture || !m_ssaoBlendShader || !m_ssaoBlendShader->IsValid()) {
        return hdrScene;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    rhi::IRenderTarget* targetRT = SelectPostTargetHDR(hdrScene);
    if (!cmd || !targetRT) return hdrScene;
    rhi::BarrierTextureForShaderRead(cmd, hdrScene);
    rhi::BarrierTextureForShaderRead(cmd, ssaoTexture);
    cmd->SetRenderTarget(targetRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);
    m_ssaoBlendShader->Bind(m_deviceRHI);
    m_ssaoBlendShader->SetInt("u_Scene", 0);
    m_ssaoBlendShader->SetInt("u_SSAO", 1);
    cmd->BindTexture(hdrScene, 0);
    cmd->BindTexture(ssaoTexture, 1);
    if (m_ssaoBlendShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_ssaoBlendShader->GetPipelineRHI()); }
    m_ssaoBlendShader->Unbind();
    return GetColorTexture(targetRT);
}

rhi::ITexture* PostProcessor::ApplyBloomCompositeHDR(rhi::ITexture* hdrScene, rhi::ITexture* bloomTexture, const Settings& settings) {
    if (!hdrScene || !bloomTexture || !m_bloomCompositeShader || !m_bloomCompositeShader->IsValid()) {
        return hdrScene;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    rhi::IRenderTarget* compositeRT = SelectPostTargetHDR(hdrScene);
    if (!cmd || !compositeRT) return hdrScene;
    rhi::BarrierTextureForShaderRead(cmd, hdrScene);
    rhi::BarrierTextureForShaderRead(cmd, bloomTexture);
    cmd->SetRenderTarget(compositeRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);
    m_bloomCompositeShader->Bind(m_deviceRHI);
    m_bloomCompositeShader->SetInt("u_SceneTexture", 0);
    m_bloomCompositeShader->SetInt("u_BloomTexture", 1);
    m_bloomCompositeShader->SetFloat("u_BloomIntensity", settings.bloomIntensity);
    if (m_postPassLayout) {
        rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, hdrScene}, {1u, bloomTexture}});
        if (postSet) {
            if (rhi::IPipeline* pipe = m_bloomCompositeShader->GetPipelineRHI())
                cmd->BindPipeline(pipe);
            BindPostPassDescriptorSets(cmd, postSet);
            abi::WriteBloomCompositePushConstant(cmd, settings.bloomIntensity);
        } else {
            m_bloomCompositeShader->Unbind();
            return hdrScene;
        }
    } else {
        cmd->BindTexture(hdrScene, 0);
        cmd->BindTexture(bloomTexture, 1);
    }
    if (m_bloomCompositeShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_bloomCompositeShader->GetPipelineRHI()); }
    m_bloomCompositeShader->Unbind();
    return GetColorTexture(compositeRT);
}

 rhi::ITexture* PostProcessor::ApplyFXAA(rhi::ITexture* texture) {
    if (!m_fxaaShader || !m_fxaaShader->IsValid()) {
        return texture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    rhi::IRenderTarget* targetRT = SelectPostTargetLDR(texture);
    if (!cmd || !targetRT) return texture;
    rhi::BarrierTextureForShaderRead(cmd, texture);
    cmd->SetRenderTarget(targetRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);
    m_fxaaShader->Bind(m_deviceRHI);
    m_fxaaShader->SetInt("u_Texture", 0);
    m_fxaaShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    if (m_postPassLayout) {
        rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, texture}});
        if (postSet) {
            if (rhi::IPipeline* pipe = m_fxaaShader->GetPipelineRHI())
                cmd->BindPipeline(pipe);
            BindPostPassDescriptorSets(cmd, postSet);
            abi::WriteFxaaPushConstants(cmd, static_cast<float>(m_width), static_cast<float>(m_height));
        } else {
            m_fxaaShader->Unbind();
            return texture;
        }
    } else {
        cmd->BindTexture(texture, 0);
    }
    if (m_fxaaShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_fxaaShader->GetPipelineRHI()); }
    m_fxaaShader->Unbind();
    m_lastOutputTexture = GetColorTexture(targetRT);
    m_lastOutputRT = targetRT;
    return m_lastOutputTexture;
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

 rhi::ITexture* PostProcessor::ApplySSAO(const GBuffer& gbuffer, const Settings& settings, const math::Mat4& viewMatrix,
                                         const math::Mat4& projectionMatrix) {
    if (!m_ssaoShader || !m_ssaoShader->IsValid()) {
        return nullptr;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    if (!cmd || !m_ssaoRT) return nullptr;
    rhi::ClearValue cv = {1.f, 1.f, 1.f, 1.f};
    rhi::ClearAndSetRenderTargetWithViewport(cmd, m_ssaoRT, 0, 0, m_width, m_height, cv);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);

    m_ssaoShader->Bind(m_deviceRHI);
    if (m_postPassLayout) {
        rhi::ITexture* posT = gbuffer.GetPositionTextureRHI();
        rhi::ITexture* nrmT = gbuffer.GetNormalTextureRHI();
        rhi::ITexture* depT = gbuffer.GetDepthTextureRHI();
        rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, posT}, {1u, nrmT}, {2u, depT}});
        if (postSet && m_ssaoShader->GetPipelineRHI()) {
            cmd->BindPipeline(m_ssaoShader->GetPipelineRHI());
            BindPostPassDescriptorSets(cmd, postSet);
            abi::WriteSsaoPushConstants(cmd, viewMatrix, projectionMatrix, static_cast<float>(m_width), static_cast<float>(m_height),
                                        settings.ssaoRadius, settings.ssaoBias, settings.ssaoSamples);
        }
    } else {
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
    }
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
            if (m_postPassLayout) {
                rhi::IDescriptorSet* blurSet = CreateTransientPostSet({{0u, srcTex}, {1u, depthTex}});
                if (blurSet && m_ssaoBlurShader->GetPipelineRHI()) {
                    cmd->BindPipeline(m_ssaoBlurShader->GetPipelineRHI());
                    BindPostPassDescriptorSets(cmd, blurSet);
                    abi::WriteSsaoBlurPushConstants(cmd, (pass == 0) ? 1.0f : 0.0f, (pass == 0) ? 0.0f : 1.0f, res.x, res.y, 0.1f,
                                                      1000.0f, farOverEdge, kSSAOBlurSampleCount, kernel, kSSAOBlurKernelSize);
                }
            } else {
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
            }
            if (m_ssaoBlurShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_ssaoBlurShader->GetPipelineRHI()); }
            m_ssaoBlurShader->Unbind();
        }
    }

    return GetColorTexture(m_ssaoRT);
}

void PostProcessor::BuildDepthPyramid(const GBuffer& gbuffer, float nearPlane, float farPlane) {
    if (!m_depthPyramidShader || !m_depthPyramidShader->IsValid() || m_depthPyramidRTs.size() != kDepthPyramidLevels) return;
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    rhi::ITexture* depthTex = gbuffer.GetDepthTextureRHI();
    if (!cmd || !depthTex) return;

    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    m_depthPyramidShader->Bind(m_deviceRHI);

    const bool usePostLayout = (m_postPassLayout != nullptr);
    if (!usePostLayout) {
        m_depthPyramidShader->SetFloat("u_NearPlane", nearPlane);
        m_depthPyramidShader->SetFloat("u_FarPlane", farPlane);
    }

    // Level 0: copy linearized depth from gbuffer
    if (!usePostLayout) {
        m_depthPyramidShader->SetInt("u_SourceDepth", 0);
        m_depthPyramidShader->SetInt("u_Pyramid", 1);
        m_depthPyramidShader->SetInt("u_Level", 0);
        m_depthPyramidShader->SetVec2("u_SourceSize", math::Vec2(static_cast<float>(m_width), static_cast<float>(m_height)));
    }
    if (usePostLayout && m_depthPyramidTexture) {
        rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, depthTex}, {1u, m_depthPyramidTexture}});
        if (postSet && m_depthPyramidShader->GetPipelineRHI()) {
            cmd->BindPipeline(m_depthPyramidShader->GetPipelineRHI());
            BindPostPassDescriptorSets(cmd, postSet);
            abi::WriteDepthPyramidPushConstants(cmd, 0, static_cast<float>(m_width), static_cast<float>(m_height), nearPlane, farPlane);
        }
    } else {
        cmd->BindTexture(depthTex, 0);
        if (m_depthPyramidTexture) cmd->BindTexture(m_depthPyramidTexture, 1);
    }
    cmd->SetRenderTarget(m_depthPyramidRTs[0]);
    cmd->SetViewport(0, 0, m_width, m_height);
    if (m_depthPyramidShader->GetPipelineRHI()) cmd->DrawFullscreenQuad(m_depthPyramidShader->GetPipelineRHI());

    if (m_depthPyramidTexture)
        rhi::BarrierTextureForShaderRead(cmd, m_depthPyramidTexture);

    // Levels 1..k-1: min of 2x2 from previous mip
    for (uint32_t mip = 1; mip < kDepthPyramidLevels; ++mip) {
        uint32_t prevW = std::max(1u, m_width >> (mip - 1));
        uint32_t prevH = std::max(1u, m_height >> (mip - 1));
        uint32_t currW = std::max(1u, m_width >> mip);
        uint32_t currH = std::max(1u, m_height >> mip);
        if (!usePostLayout) {
            m_depthPyramidShader->SetInt("u_Level", static_cast<int>(mip));
            m_depthPyramidShader->SetVec2("u_SourceSize", math::Vec2(static_cast<float>(prevW), static_cast<float>(prevH)));
        }
        if (usePostLayout && m_depthPyramidTexture) {
            rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, depthTex}, {1u, m_depthPyramidTexture}});
            if (postSet && m_depthPyramidShader->GetPipelineRHI()) {
                cmd->BindPipeline(m_depthPyramidShader->GetPipelineRHI());
                BindPostPassDescriptorSets(cmd, postSet);
                abi::WriteDepthPyramidPushConstants(cmd, static_cast<int>(mip), static_cast<float>(prevW), static_cast<float>(prevH), nearPlane,
                                                      farPlane);
            }
        } else {
            cmd->BindTexture(m_depthPyramidTexture, 1);
        }
        cmd->SetRenderTarget(m_depthPyramidRTs[mip]);
        cmd->SetViewport(0, 0, currW, currH);
        if (m_depthPyramidShader->GetPipelineRHI()) cmd->DrawFullscreenQuad(m_depthPyramidShader->GetPipelineRHI());
        if (m_depthPyramidTexture)
            rhi::BarrierTextureForShaderRead(cmd, m_depthPyramidTexture);
    }
    m_depthPyramidShader->Unbind();
}

 rhi::ITexture* PostProcessor::ApplySSR(rhi::ITexture* sceneTexture, const GBuffer& gbuffer, rhi::ITexture* skyboxTexture, const Settings& settings,
                                       const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                       const math::Mat4& invViewMatrix, const math::Mat4& invProjectionMatrix,
                                       const math::Vec3& viewPos, rhi::IRenderTarget* outputRT, rhi::ITexture* reflectionProbeTexture) {
    if (!m_ssrShader || !m_ssrShader->IsValid()) {
        return sceneTexture;
    }
    rhi::IRenderTarget* rt = (outputRT != nullptr) ? outputRT : m_outputRT;
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    if (!cmd || !rt) return sceneTexture;

    rhi::BarrierTextureForShaderRead(cmd, sceneTexture);
    if (skyboxTexture) rhi::BarrierTextureForShaderRead(cmd, skyboxTexture);
    if (reflectionProbeTexture) rhi::BarrierTextureForShaderRead(cmd, reflectionProbeTexture);

    // Build Hi-Z depth pyramid for faster ray march
    BuildDepthPyramid(gbuffer, settings.ssrNearPlane, settings.ssrFarPlane);

    cmd->SetRenderTarget(rt);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);

    m_ssrShader->Bind(m_deviceRHI);
    if (m_postPassLayout) {
        rhi::ITexture* posT = gbuffer.GetPositionTextureRHI();
        rhi::ITexture* nrmT = gbuffer.GetNormalTextureRHI();
        rhi::ITexture* albT = gbuffer.GetAlbedoTextureRHI();
        rhi::ITexture* depT = gbuffer.GetDepthTextureRHI();
        rhi::ITexture* pbrT = gbuffer.GetPBRParamsTextureRHI() ? gbuffer.GetPBRParamsTextureRHI() : albT;
        if (!posT || !nrmT || !albT || !depT || !skyboxTexture || !m_ssrShader->GetPipelineRHI()) {
            m_ssrShader->Unbind();
            return sceneTexture;
        }
        rhi::IDescriptorSet* postSet =
            CreateTransientPostSet({{0u, posT}, {1u, nrmT}, {2u, albT}, {3u, pbrT}, {4u, depT}, {5u, sceneTexture}, {6u, skyboxTexture}});
        if (!postSet) {
            m_ssrShader->Unbind();
            return sceneTexture;
        }
        cmd->BindPipeline(m_ssrShader->GetPipelineRHI());
        BindPostPassDescriptorSets(cmd, postSet);
        abi::WriteSSRPushConstants(cmd, viewMatrix, projectionMatrix, invViewMatrix, viewPos, settings.ssrNearPlane, settings.ssrFarPlane,
                                   static_cast<float>(m_width), static_cast<float>(m_height), settings.ssrStepSize, settings.ssrThickness,
                                   settings.ssrMaxSteps);
    } else {
        m_ssrShader->SetInt("g_Position", 0);
        m_ssrShader->SetInt("g_Normal", 1);
        m_ssrShader->SetInt("g_Albedo", 2);
        m_ssrShader->SetInt("g_PBRParams", 3);
        m_ssrShader->SetInt("g_Depth", 4);
        m_ssrShader->SetInt("u_SceneTexture", 5);
        m_ssrShader->SetInt("u_Skybox", 6);
        m_ssrShader->SetInt("u_ReflectionProbe", 7);
        m_ssrShader->SetInt("u_PrevSSR", 8);
        m_ssrShader->SetInt("u_DepthPyramid", 9);
        m_ssrShader->SetInt("u_HasReflectionProbe", reflectionProbeTexture ? 1 : 0);
        m_ssrShader->SetInt("u_TemporalEnabled", m_ssrHasPrevFrame && m_ssrPrevRT ? 1 : 0);
        m_ssrShader->SetInt("u_HasDepthPyramid", m_depthPyramidTexture ? 1 : 0);
        m_ssrShader->SetInt("u_PyramidLevels", static_cast<int>(kDepthPyramidLevels));
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
        if (reflectionProbeTexture) cmd->BindTexture(reflectionProbeTexture, 7);
        if (m_ssrHasPrevFrame && m_ssrPrevRT) {
            rhi::ITexture* prevTex = GetColorTexture(m_ssrPrevRT);
            if (prevTex) cmd->BindTexture(prevTex, 8);
        }
        if (m_depthPyramidTexture) cmd->BindTexture(m_depthPyramidTexture, 9);
    }
    if (m_ssrShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_ssrShader->GetPipelineRHI()); }
    m_ssrShader->Unbind();

    // Copy result to previous-frame buffer for temporal accumulation
    if (m_ssrPrevRT && m_copyShader && m_copyShader->IsValid()) {
        rhi::ITexture* ssrOut = GetColorTexture(rt);
        if (ssrOut) {
            cmd->SetRenderTarget(m_ssrPrevRT);
            cmd->SetViewport(0, 0, m_width, m_height);
            cmd->SetDepthTest(false);
            cmd->SetDepthWrite(false);
            m_copyShader->Bind(m_deviceRHI);
            m_copyShader->SetInt("u_Texture", 0);
            if (m_postPassLayout) {
                rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, ssrOut}});
                if (postSet) {
                    if (rhi::IPipeline* pipe = m_copyShader->GetPipelineRHI())
                        cmd->BindPipeline(pipe);
                    BindPostPassDescriptorSets(cmd, postSet);
                } else {
                    m_copyShader->Unbind();
                    return ssrOut;
                }
            } else {
                cmd->BindTexture(ssrOut, 0);
            }
            if (m_copyShader->GetPipelineRHI()) cmd->DrawFullscreenQuad(m_copyShader->GetPipelineRHI());
            m_copyShader->Unbind();
            m_ssrHasPrevFrame = true;
        }
    }

    return GetColorTexture(rt);
}

 rhi::ITexture* PostProcessor::ApplyMotionBlur(rhi::ITexture* texture, rhi::ITexture* velocityTexture, const Settings& settings) {
    if (!m_motionBlurShader || !m_motionBlurShader->IsValid() || !velocityTexture) {
        return texture;  // Skip if no shader or no velocity buffer
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    rhi::IRenderTarget* targetRT = SelectPostTargetLDR(texture);
    if (!cmd || !targetRT) return texture;
    rhi::BarrierTextureForShaderRead(cmd, texture);
    rhi::BarrierTextureForShaderRead(cmd, velocityTexture);
    cmd->SetRenderTarget(targetRT);
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
    if (m_postPassLayout) {
        rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, texture}, {2u, velocityTexture}});
        if (postSet) {
            if (rhi::IPipeline* pipe = m_motionBlurShader->GetPipelineRHI())
                cmd->BindPipeline(pipe);
            BindPostPassDescriptorSets(cmd, postSet);
            abi::WriteMotionBlurPushConstants(cmd, static_cast<float>(m_width), static_cast<float>(m_height),
                                              settings.motionBlurIntensity, settings.motionBlurSamples);
        } else {
            m_motionBlurShader->Unbind();
            return texture;
        }
    } else {
        cmd->BindTexture(texture, 0);
        cmd->BindTexture(velocityTexture, 1);
    }
    if (m_motionBlurShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_motionBlurShader->GetPipelineRHI()); }
    m_motionBlurShader->Unbind();
    m_lastOutputTexture = GetColorTexture(targetRT);
    m_lastOutputRT = targetRT;
    return m_lastOutputTexture;
}

 rhi::ITexture* PostProcessor::ApplyVignette(rhi::ITexture* texture, const Settings& settings) {
    if (!m_vignetteShader || !m_vignetteShader->IsValid()) {
        return texture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    rhi::IRenderTarget* targetRT = SelectPostTargetLDR(texture);
    if (!cmd || !targetRT) return texture;
    rhi::BarrierTextureForShaderRead(cmd, texture);
    cmd->SetRenderTarget(targetRT);
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
    if (m_postPassLayout) {
        rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, texture}});
        if (postSet) {
            if (rhi::IPipeline* pipe = m_vignetteShader->GetPipelineRHI())
                cmd->BindPipeline(pipe);
            BindPostPassDescriptorSets(cmd, postSet);
            abi::WriteVignettePushConstants(cmd, settings.vignetteIntensity, settings.vignetteRadius, settings.vignetteSoftness);
        } else {
            m_vignetteShader->Unbind();
            return texture;
        }
    } else {
        cmd->BindTexture(texture, 0);
    }
    if (m_vignetteShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_vignetteShader->GetPipelineRHI()); }
    m_vignetteShader->Unbind();
    m_lastOutputTexture = GetColorTexture(targetRT);
    m_lastOutputRT = targetRT;
    return m_lastOutputTexture;
}

 rhi::ITexture* PostProcessor::ApplyFilmGrain(rhi::ITexture* texture, const Settings& settings, float time) {
    if (!m_filmGrainShader || !m_filmGrainShader->IsValid()) {
        return texture;
    }
    rhi::ICommandList* cmd = m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr;
    rhi::IRenderTarget* targetRT = SelectPostTargetLDR(texture);
    if (!cmd || !targetRT) return texture;
    rhi::BarrierTextureForShaderRead(cmd, texture);
    cmd->SetRenderTarget(targetRT);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->SetBlend(false);
    m_filmGrainShader->Bind(m_deviceRHI);
    m_filmGrainShader->SetInt("u_Texture", 0);
    m_filmGrainShader->SetFloat("u_Intensity", settings.filmGrainIntensity);
    m_filmGrainShader->SetFloat("u_Time", time);
    if (m_postPassLayout) {
        rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, texture}});
        if (postSet) {
            if (rhi::IPipeline* pipe = m_filmGrainShader->GetPipelineRHI())
                cmd->BindPipeline(pipe);
            BindPostPassDescriptorSets(cmd, postSet);
            abi::WriteFilmGrainPushConstants(cmd, settings.filmGrainIntensity, time);
        } else {
            m_filmGrainShader->Unbind();
            return texture;
        }
    } else {
        cmd->BindTexture(texture, 0);
    }
    if (m_filmGrainShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_filmGrainShader->GetPipelineRHI()); }
    m_filmGrainShader->Unbind();
    m_lastOutputTexture = GetColorTexture(targetRT);
    m_lastOutputRT = targetRT;
    return m_lastOutputTexture;
}

void PostProcessor::AddPostPassesToFrameGraph(FrameGraph* fg, int hdrInput, int toneMappedOutput, int finalOutput,
                                              const Settings& settings, rhi::ICommandList* cmd) {
    if (!fg || !cmd || !m_pingPongRT[0] || !m_outputRT) return;

    fg->AddPass("Post.ToneMap",
        [this, hdrInput, settings, cmd](const FGPassResources& res) {
            rhi::ITexture* hdrTex = res.getTextureRHI(hdrInput);
            if (!hdrTex || !m_toneMappingShader || !m_toneMappingShader->IsValid()) return;
            rhi::IRenderTarget* toneTarget = SelectPostTargetLDR(hdrTex);
            if (!toneTarget) return;
            cmd->SetRenderTarget(toneTarget);
            cmd->SetViewport(0, 0, res.width, res.height);
            cmd->SetDepthTest(false);
            cmd->SetDepthWrite(false);
            cmd->SetCullFace(false);
            cmd->SetBlend(false);
            rhi::ClearValue clearBlack = {0.f, 0.f, 0.f, 1.f};
            cmd->ClearRenderTarget(toneTarget, clearBlack);
            m_toneMappingShader->Bind(m_deviceRHI);
            if (m_postPassLayout) {
                rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, hdrTex}});
                if (postSet) {
                    if (rhi::IPipeline* pipe = m_toneMappingShader->GetPipelineRHI())
                        cmd->BindPipeline(pipe);
                    BindPostPassDescriptorSets(cmd, postSet);
                    abi::WriteTonemapPushConstants(cmd, settings.exposure, settings.useACES ? 1 : 0);
                } else {
                    m_toneMappingShader->Unbind();
                    return;
                }
            } else {
                m_toneMappingShader->SetInt("u_HDRTexture", 0);
                m_toneMappingShader->SetFloat("u_Exposure", settings.exposure);
                m_toneMappingShader->SetBool("u_UseACES", settings.useACES);
                cmd->BindTexture(hdrTex, 0);
            }
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
            if (m_postPassLayout) {
                rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, toneMappedTex}});
                if (postSet) {
                    if (rhi::IPipeline* pipe = m_fxaaShader->GetPipelineRHI())
                        cmd->BindPipeline(pipe);
                    BindPostPassDescriptorSets(cmd, postSet);
                    abi::WriteFxaaPushConstants(cmd, static_cast<float>(res.width), static_cast<float>(res.height));
                } else {
                    m_fxaaShader->Unbind();
                    return;
                }
            } else {
                m_fxaaShader->SetInt("u_Texture", 0);
                m_fxaaShader->SetVec2("u_Resolution", math::Vec2(static_cast<float>(res.width), static_cast<float>(res.height)));
                cmd->BindTexture(toneMappedTex, 0);
            }
            if (m_fxaaShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_fxaaShader->GetPipelineRHI()); }
            m_fxaaShader->Unbind();
        },
        { toneMappedOutput }, { finalOutput }, true);

    fg->AddPass("Post.DisplayEncode",
        [this, finalOutput, cmd](const FGPassResources& res) {
            rhi::ITexture* tex = res.getTextureRHI(finalOutput);
            if (!tex || !m_displayEncodeShader || !m_displayEncodeShader->IsValid()) return;
            rhi::IRenderTarget* targetRT = SelectPostTargetLDR(tex);
            if (!targetRT) return;
            cmd->SetRenderTarget(targetRT);
            cmd->SetViewport(0, 0, res.width, res.height);
            cmd->SetDepthTest(false);
            cmd->SetDepthWrite(false);
            cmd->SetCullFace(false);
            cmd->SetBlend(false);
            m_displayEncodeShader->Bind(m_deviceRHI);
            m_displayEncodeShader->SetInt("u_Texture", 0);
            if (m_postPassLayout) {
                rhi::IDescriptorSet* postSet = CreateTransientPostSet({{0u, tex}});
                if (postSet) {
                    if (rhi::IPipeline* pipe = m_displayEncodeShader->GetPipelineRHI())
                        cmd->BindPipeline(pipe);
                    BindPostPassDescriptorSets(cmd, postSet);
                } else {
                    m_displayEncodeShader->Unbind();
                    return;
                }
            } else {
                cmd->BindTexture(tex, 0);
            }
            if (m_displayEncodeShader->GetPipelineRHI()) { cmd->DrawFullscreenQuad(m_displayEncodeShader->GetPipelineRHI()); }
            m_displayEncodeShader->Unbind();
            m_lastOutputValid = true;
        },
        { finalOutput }, { finalOutput }, true);
}

rhi::ITexture* PostProcessor::GetColorTexture(rhi::IRenderTarget* rt) const {
    return rt ? rt->GetColorTexture(0) : nullptr;
}

rhi::IRenderTarget* PostProcessor::SelectPostTargetLDR(rhi::ITexture* inputTexture) const {
    if (!m_outputRT || !m_ldrPingPong) return nullptr;
    if (!inputTexture) return m_outputRT;
    if (GetColorTexture(m_outputRT) != inputTexture) return m_outputRT;
    if (GetColorTexture(m_ldrPingPong) != inputTexture) return m_ldrPingPong;
    return nullptr;
}

rhi::IRenderTarget* PostProcessor::SelectPostTargetHDR(rhi::ITexture* inputTexture) const {
    if (!m_pingPongRT[0] || !m_pingPongRT[1]) return nullptr;
    if (!inputTexture) return m_pingPongRT[0];
    if (GetColorTexture(m_pingPongRT[0]) != inputTexture) return m_pingPongRT[0];
    if (GetColorTexture(m_pingPongRT[1]) != inputTexture) return m_pingPongRT[1];
    return nullptr;
}

rhi::IRenderTarget* PostProcessor::SelectPostTarget(rhi::ITexture* inputTexture) const {
    if (!m_outputRT) return nullptr;
    if (!inputTexture) return m_outputRT;
    if (GetColorTexture(m_outputRT) != inputTexture) return m_outputRT;
    if (m_pingPongRT[0] && GetColorTexture(m_pingPongRT[0]) != inputTexture) return m_pingPongRT[0];
    if (m_pingPongRT[1] && GetColorTexture(m_pingPongRT[1]) != inputTexture) return m_pingPongRT[1];
    return nullptr;
}

rhi::IRenderTarget* PostProcessor::ResolveRenderTargetForTexture(rhi::ITexture* texture) const {
    if (!texture) return nullptr;
    if (m_outputRT && GetColorTexture(m_outputRT) == texture) return m_outputRT;
    if (m_ldrPingPong && GetColorTexture(m_ldrPingPong) == texture) return m_ldrPingPong;
    if (m_pingPongRT[0] && GetColorTexture(m_pingPongRT[0]) == texture) return m_pingPongRT[0];
    if (m_pingPongRT[1] && GetColorTexture(m_pingPongRT[1]) == texture) return m_pingPongRT[1];
    if (m_bloomExtractRT && GetColorTexture(m_bloomExtractRT) == texture) return m_bloomExtractRT;
    if (m_bloomBlurScratchRT && GetColorTexture(m_bloomBlurScratchRT) == texture) return m_bloomBlurScratchRT;
    if (m_ssaoRT && GetColorTexture(m_ssaoRT) == texture) return m_ssaoRT;
    if (m_ssaoBlurRT && GetColorTexture(m_ssaoBlurRT) == texture) return m_ssaoBlurRT;
    if (m_ssrPrevRT && GetColorTexture(m_ssrPrevRT) == texture) return m_ssrPrevRT;
    return nullptr;
}

rhi::IRenderTarget* PostProcessor::GetOutputRenderTargetRHI() const {
    if (m_lastOutputRT) return m_lastOutputRT;
    if (m_lastOutputTexture) {
        if (rhi::IRenderTarget* resolved = ResolveRenderTargetForTexture(m_lastOutputTexture))
            return resolved;
    }
    return m_outputRT;
}

rhi::ITexture* PostProcessor::GetOutputTextureRHI() const {
    if (m_lastOutputTexture) return m_lastOutputTexture;
    return GetColorTexture(m_outputRT);
}

} // namespace render
} // namespace eyelib
