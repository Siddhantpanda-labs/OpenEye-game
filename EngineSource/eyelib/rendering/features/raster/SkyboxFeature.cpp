#include "eyelib/rendering/features/raster/SkyboxFeature.h"

#include "eyelib/core/Log.h"
#include "eyelib/rendering/features/raster/RasterFeature.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/BlackboardWriterRegistry.h"
#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/lighting/ibl/IBL.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"
#include "eyelib/rendering/scene/CameraView.h"
#include "eyelib/rendering/scene/IRenderSceneView.h"
#include "eyelib/rendering/shaders/ShaderPermutationKey.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace eyelib {
namespace rendering {

namespace {

const shaders::ShaderPermutationKey kPermSkyboxCubemap{
    shaders::ShaderPermutationKey::ProgramId::SkyboxCubemap, 1, 1};

const shaders::ShaderPermutationKey kPermSkyboxProcedural{
    shaders::ShaderPermutationKey::ProgramId::SkyboxProcedural, 1, 1};

/** Resolution of the procedural-sky environment cubemap captured for IBL bake. 128² is the
 *  same fidelity tier the legacy stack used for its default env probe; the irradiance bake
 *  immediately convolves it down to 32² and the prefilter bake to 512²-with-mips, so a
 *  higher capture resolution offers no meaningful quality win for an analytic gradient. */
constexpr uint32_t kProceduralEnvCubeSize = 128u;

/** Push-constant layout for the procedural sky shader. Must match the layout in
 *  `passes/sky/skybox_procedural.frag` (and `SkyboxPass::SkyboxPushConstants`). */
struct ProceduralCapturePush {
    math::Mat4 inverseView;       // offset   0
    math::Mat4 inverseProjection; // offset  64
    float      brightness;        // offset 128
    float      pad0;
    float      pad1;
    float      pad2;
};

}  // namespace

SkyboxFeature::~SkyboxFeature() {
    m_skyboxPass.reset();
    DestroyBakedProbes(m_pipelineDevice);
    DestroyProceduralCapture(m_pipelineDevice);
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
    m_pipelineDevice = nullptr;
    m_defaultEnvProvider = nullptr;
}

void SkyboxFeature::DestroyBakedProbes(rhi::IDevice* device) noexcept {
    if (device) {
        if (m_bakedIrradiance) device->DestroyTexture(m_bakedIrradiance);
        if (m_bakedPrefilter)  device->DestroyTexture(m_bakedPrefilter);
    }
    m_bakedIrradiance = nullptr;
    m_bakedPrefilter  = nullptr;
    m_bakedSource     = nullptr;
}

void SkyboxFeature::DestroyProceduralCapture(rhi::IDevice* device) noexcept {
    if (device) {
        if (m_proceduralEnvCube)         device->DestroyTexture(m_proceduralEnvCube);
        if (m_proceduralCapturePipeline) device->DestroyPipeline(m_proceduralCapturePipeline);
    }
    m_proceduralEnvCube         = nullptr;
    m_proceduralCapturePipeline = nullptr;
}

rhi::ShaderGraphicsPipelineCreateInfo SkyboxFeature::MakeCubemapSpec() {
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    gp.assetPath = "passes/sky/skybox_cubemap";
    // No vertex layout — full-screen triangle generated from gl_VertexIndex (matches the
    // deferred-lighting full-screen path).
    gp.topology = rhi::PrimitiveTopology::TriangleList;
    gp.layoutKind = rhi::PipelineLayoutKind::IBL;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = true;
    gp.depthStencilOverride.depthWrite = false;
    gp.depthStencilOverride.depthCompare = rhi::DepthCompare::LessEqual;
    rhi::PipelineAttachmentFormats att{};
    att.color.push_back(rhi::RHITextureFormat::RGBA16F);
    att.depthStencil = rhi::RHITextureFormat::Depth24Stencil8;
    gp.attachmentFormats = att;
    return gp;
}

rhi::ShaderGraphicsPipelineCreateInfo SkyboxFeature::MakeProceduralSpec() {
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    gp.assetPath = "passes/sky/skybox_procedural";
    gp.topology = rhi::PrimitiveTopology::TriangleList;
    gp.layoutKind = rhi::PipelineLayoutKind::PushOnly;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = true;
    gp.depthStencilOverride.depthWrite = false;
    gp.depthStencilOverride.depthCompare = rhi::DepthCompare::LessEqual;
    rhi::PipelineAttachmentFormats att{};
    att.color.push_back(rhi::RHITextureFormat::RGBA16F);
    att.depthStencil = rhi::RHITextureFormat::Depth24Stencil8;
    gp.attachmentFormats = att;
    return gp;
}

rhi::ShaderGraphicsPipelineCreateInfo SkyboxFeature::MakeProceduralCaptureSpec() {
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    // Same shader asset as the on-screen procedural pass — only the pipeline state differs.
    gp.assetPath = "passes/sky/skybox_procedural";
    gp.topology = rhi::PrimitiveTopology::TriangleList;
    gp.layoutKind = rhi::PipelineLayoutKind::PushOnly;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;
    // The capture cubemap has no depth attachment, so depth must be off and the pipeline
    // must declare `depthStencil = None` to satisfy Vulkan dynamic-rendering format matching.
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = false;
    gp.depthStencilOverride.depthWrite = false;
    rhi::PipelineAttachmentFormats att{};
    att.color.push_back(rhi::RHITextureFormat::RGBA16F);
    att.depthStencil = rhi::RHITextureFormat::None;
    gp.attachmentFormats = att;
    return gp;
}

rhi::ITexture* SkyboxFeature::EnsureProceduralEnvCube(rhi::IDevice* device, rhi::ICommandList* cmd) {
    if (!device || !cmd) return nullptr;
    if (m_proceduralEnvCube) return m_proceduralEnvCube;  // Captured once and reused.

    if (!m_proceduralCapturePipeline) {
        m_proceduralCapturePipeline = device->CreateGraphicsPipelineFromShaderAsset(MakeProceduralCaptureSpec());
        if (!m_proceduralCapturePipeline) {
            core::Log::Warn("SkyboxFeature: failed to create procedural-sky capture pipeline; IBL will use fallback env.");
            return nullptr;
        }
    }

    rhi::TextureDesc desc;
    desc.type      = rhi::TextureType::TextureCube;
    desc.width     = kProceduralEnvCubeSize;
    desc.height    = kProceduralEnvCubeSize;
    desc.format    = rhi::RHITextureFormat::RGBA16F;
    desc.usage     = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::RenderTarget;
    desc.mipLevels = 1;
    desc.debugName = "SkyboxFeature.ProceduralEnvCube";
    m_proceduralEnvCube = device->CreateTexture(desc, nullptr);
    if (!m_proceduralEnvCube) {
        core::Log::Warn("SkyboxFeature: failed to allocate procedural env cubemap; IBL will use fallback env.");
        return nullptr;
    }

    // Same capture frame as `IBL::GenerateIrradianceAndPrefilter` so the per-face NDC→world
    // reconstruction in the procedural shader (`uInverseView`, `uInverseProjection`) maps the
    // fullscreen triangle's UV span to the corresponding cubemap face direction.
    const math::Mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    const math::Mat4 captureViews[] = {
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 1.0f,  0.0f,  0.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3(-1.0f,  0.0f,  0.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  1.0f,  0.0f), math::Vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f, -1.0f,  0.0f), math::Vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  0.0f,  1.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  0.0f, -1.0f), math::Vec3(0.0f, -1.0f,  0.0f))
    };

    const math::Mat4 invProjection = glm::inverse(captureProjection);

    for (uint32_t face = 0; face < 6; ++face) {
        rhi::IRenderTarget* rt = device->CreateRenderTargetFromTexture(m_proceduralEnvCube, face, 0);
        if (!rt) continue;
        cmd->SetRenderTarget(rt);
        cmd->SetViewport(0, 0, kProceduralEnvCubeSize, kProceduralEnvCubeSize);
        rhi::ClearValue clear{};
        cmd->ClearRenderTarget(rt, clear);
        cmd->SetDepthTest(false);
        cmd->SetDepthWrite(false);
        cmd->SetCullFace(false);
        cmd->BindPipeline(m_proceduralCapturePipeline);

        ProceduralCapturePush pc{};
        pc.inverseView       = glm::inverse(captureViews[face]);
        pc.inverseProjection = invProjection;
        // Brightness 1.0 — IBL bake input lives in linear HDR space; the on-screen sky pass
        // applies its own brightness multiplier separately, so we must not pre-scale here.
        pc.brightness        = 1.0f;
        cmd->SetPushConstants(0, sizeof(ProceduralCapturePush), &pc);

        // Full-screen triangle generated from gl_VertexIndex; no VBO bound.
        cmd->Draw(3, 0);
        cmd->SetRenderTarget(nullptr);
        device->DestroyRenderTarget(rt);
    }

    // Hand off to downstream IBL bake / per-frame sampling in shader-resource state to match
    // the legacy IBL flow (see `IBL::Initialize` end-of-bake barriers).
    cmd->BarrierTexture(m_proceduralEnvCube, rhi::ResourceState::RenderTarget,
                        rhi::ResourceState::ShaderResource);

    return m_proceduralEnvCube;
}

void SkyboxFeature::OnRegister(const RenderCapabilities& caps) {
    (void)caps;
    m_shaderLibrary.ClearRegistrations();
    m_shaderLibrary.AddEntry(kPermSkyboxCubemap, MakeCubemapSpec());
    m_shaderLibrary.AddEntry(kPermSkyboxProcedural, MakeProceduralSpec());
}

void SkyboxFeature::OnRegisterBlackboardWriters(BlackboardWriterRegistry& registry, const RenderCapabilities& caps) {
    (void)caps;
    // Ownership of the diffuse + specular probe slots lives here so the per-scene cubemap
    // bake can drive the lighting pass's IBL term. BRDF LUT stays on `RasterFeature`.
    (void)registry.TryReserve(BlackboardSlot::IblIrradianceCubemap, writer_names::kSkyboxFeature);
    (void)registry.TryReserve(BlackboardSlot::IblPrefilterCubemap, writer_names::kSkyboxFeature);
}

void SkyboxFeature::OnUnregisterBlackboardWriters(BlackboardWriterRegistry& registry) {
    (void)registry.TryRelease(BlackboardSlot::IblIrradianceCubemap, writer_names::kSkyboxFeature);
    (void)registry.TryRelease(BlackboardSlot::IblPrefilterCubemap, writer_names::kSkyboxFeature);
}

void SkyboxFeature::OnUpload(FrameContext& frame, const RenderContext& render) {
    (void)frame;
    // Per-scene IBL probe bake. The static helper records into `render.recordingCommandList`
    // (matches the legacy bake site), so this must run during the upload phase before the
    // frame graph executes the lighting pass that consumes the published probes.
    if (!render.device || !render.recordingCommandList) return;

    // Choose the environment source for the IBL bake:
    //   - Cubemap:    the user-supplied environment map (lifetime owned by the cache).
    //   - Procedural: lazily captured analytic-sky cubemap (feature-owned, captured once).
    //   - None:       no source → keep any previous bake destroyed so schedule falls back
    //                 to `RasterFeature::GetDefaultEnvCubemap()`.
    rhi::ITexture* src = nullptr;
    if (m_request.kind == SkyboxRequest::Kind::Cubemap) {
        src = m_request.cubemap;
    } else if (m_request.kind == SkyboxRequest::Kind::Procedural) {
        src = EnsureProceduralEnvCube(render.device, render.recordingCommandList);
    }

    if (!src || !src->IsGpuBindable()) {
        // No per-scene source — keep any previous bake destroyed so the fallback env is
        // exposed at schedule time. Destroying eagerly here (rather than at the next
        // successful bake) avoids holding extra GPU memory across no-sky frames.
        if (m_bakedSource) DestroyBakedProbes(render.device);
        return;
    }

    // Skip the rebake when the source pointer is unchanged. A new cubemap (different asset
    // or hot-swap) shows up as a different `ITexture*` from `EditorSkyboxCache::GetOrLoad`;
    // the procedural capture cubemap is a stable feature-owned pointer until shutdown, so it
    // bakes exactly once on the first procedural-sky frame.
    if (src == m_bakedSource && m_bakedIrradiance && m_bakedPrefilter) return;

    if (m_bakedSource) DestroyBakedProbes(render.device);

    rhi::ITexture* irradiance = nullptr;
    rhi::ITexture* prefilter  = nullptr;
    const bool ok = IBL::GenerateIrradianceAndPrefilter(render.device, src,
                                                       /*irradianceRes=*/32u,
                                                       /*prefilterRes=*/512u,
                                                       irradiance, prefilter);
    if (!ok || !irradiance || !prefilter) {
        if (irradiance) render.device->DestroyTexture(irradiance);
        if (prefilter)  render.device->DestroyTexture(prefilter);
        static int s_warnCount = 0;
        if (s_warnCount < 4) {
            ++s_warnCount;
            core::Log::Warn("SkyboxFeature: IBL probe bake failed; falling back to default env for this scene.");
        }
        return;
    }

    m_bakedSource     = src;
    m_bakedIrradiance = irradiance;
    m_bakedPrefilter  = prefilter;
}

void SkyboxFeature::OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) {
    if (!render.device) return;

    m_shaderLibrary.WarmUpPipelines(render.device);
    m_pipelineDevice = render.device;

    // Publish IBL diffuse + specular probes every frame regardless of `Kind`. Lighting
    // expects both slots populated (the previous design had `RasterFeature` covering both
    // with the default env). Preference order: baked per-scene cubemap → fallback env from
    // `RasterFeature::GetDefaultEnvCubemap()`. When neither is available we skip publishing
    // so the deferred lighting pass gets a clean "no IBL" signal rather than a stale texture.
    rhi::ITexture* iblSource = m_bakedIrradiance;
    rhi::ITexture* iblSpec   = m_bakedPrefilter;
    if (!iblSource || !iblSpec) {
        rhi::ITexture* fallback = m_defaultEnvProvider ? m_defaultEnvProvider->GetDefaultEnvCubemap() : nullptr;
        iblSource = fallback;
        iblSpec   = fallback;
    }
    if (iblSource)
        blackboard.PublishTexture(BlackboardSlot::IblIrradianceCubemap, writer_names::kSkyboxFeature, iblSource);
    if (iblSpec)
        blackboard.PublishTexture(BlackboardSlot::IblPrefilterCubemap, writer_names::kSkyboxFeature, iblSpec);

    if (m_request.kind == SkyboxRequest::Kind::None) return;

    rhi::IPipeline* cubemapPipeline = m_shaderLibrary.TryGetPipeline(kPermSkyboxCubemap);
    rhi::IPipeline* proceduralPipeline = m_shaderLibrary.TryGetPipeline(kPermSkyboxProcedural);

    // If the active kind has no pipeline yet (shader compile still warming up), skip — the
    // pass will be appended on a subsequent frame once the pipeline is ready.
    const bool needCubemap = (m_request.kind == SkyboxRequest::Kind::Cubemap);
    if (needCubemap && !cubemapPipeline) return;
    if (!needCubemap && !proceduralPipeline) return;

    if (!m_skyboxPass)
        m_skyboxPass = std::make_unique<SkyboxPass>(&blackboard, render.device, cubemapPipeline, proceduralPipeline);
    else
        m_skyboxPass->Reconfigure(&blackboard, render.device, cubemapPipeline, proceduralPipeline);

    // Inverse view / projection from the active camera. Both shader variants reconstruct a
    // world-space view direction from these matrices and `vUv`. Identity is harmless when no
    // camera is bound (sky still renders, but with a degenerate transform; the public façade
    // never reaches this point without a camera in practice).
    math::Mat4 invView{1.0f};
    math::Mat4 invProj{1.0f};
    if (const CameraView* cam = render.sceneView ? render.sceneView->TryGetCamera() : nullptr) {
        invView = glm::inverse(cam->view);
        invProj = glm::inverse(cam->projection);
    }
    m_skyboxPass->SetFrameState(m_request, invView, invProj);

    graph.AddPassBorrowed(m_skyboxPass.get());
}

void SkyboxFeature::OnShutdown() {
    m_skyboxPass.reset();
    DestroyBakedProbes(m_pipelineDevice);
    DestroyProceduralCapture(m_pipelineDevice);
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
    m_pipelineDevice = nullptr;
    m_request = {};
    m_defaultEnvProvider = nullptr;
}

void SkyboxFeature::SetRequest(const SkyboxRequest& request) noexcept {
    m_request = request;
    if (m_request.brightness < 0.0f) m_request.brightness = 0.0f;
}

}  // namespace rendering
}  // namespace eyelib
