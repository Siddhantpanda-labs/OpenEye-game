#include "eyelib/render/renderer/RenderPipeline.h"
#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/passes/GBufferPassSystem.h"
#include "eyelib/render/passes/ForwardOpaquePassSystem.h"
#include "eyelib/render/passes/ShadowPassSystem.h"
#include "eyelib/render/passes/TransparencyPass.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/renderer/graph/FrameGraphPassNames.h"
#include "eyelib/render/renderer/graph/FrameGraphRecipeBuilders.h"
#include "eyelib/render/resources/TextureCube.h"
#include "eyelib/render/rhi/RHIUtils.h"

namespace eyelib {
namespace render {

RenderContext RenderPipeline::MakeRenderContext(const FrameRenderContext& frc) const {
    RenderContext c{};
    c.device = frc.device;
    c.cmd = frc.cmd;
    return c;
}

void RenderPipeline::CompileFrameGraphIfNeeded(PipelineMode mode, bool stereo) {
    if (!m_renderGraphOrchestrator) return;
    m_renderGraphOrchestrator->CompileIfNeeded(mode, stereo, m_width, m_height, m_device,
        [this, mode, stereo](FrameGraph& fg) {
            if (mode == PipelineMode::Hybrid)
                BuildDeferredGraph(fg);
            else if (mode == PipelineMode::ForwardPlus)
                BuildForwardPlusGraph(fg);
            else if (stereo)
                BuildForwardStereoGraph(fg);
            else
                BuildForwardGraph(fg);
        });
}

void RenderPipeline::GraphExecuteGpuCull() {
    const FrameRenderContext& frc = m_frameGraphExecutionContext;
    if (m_gpuCullPassSystem)
        m_gpuCullPassSystem->ExecuteGpuCull(MakeRenderContext(frc));
}

void RenderPipeline::GraphExecuteShadow() {
    const FrameRenderContext& frc = m_frameGraphExecutionContext;
    if (!frc.frameData) return;
    ShadowPassBuildParams build = BuildShadowPassParams(frc, frc.frameData->cameraRaw);
    ExecuteShadowPass(build);
}

void RenderPipeline::GraphExecuteGBufferDeferred() {
    const FrameRenderContext& frc = m_frameGraphExecutionContext;
    if (!frc.frameData) return;
    if (!m_gbufferPassSystem) {
        m_gbufferPassSystem = std::make_unique<GBufferPassSystem>();
        m_gbufferPassSystem->SetShaderCache(m_shaderCache);
        if (m_bindlessHeap) m_gbufferPassSystem->SetBindlessHeap(m_bindlessHeap);
        m_gbufferPassSystem->SetBindlessGetHandle([this](rhi::ITexture* t) { return GetOrAllocateBindlessHandle(t); });
    }
    m_gbufferPassSystem->RenderOpaqueGeometry(
        BuildGBufferOpaqueParams(frc.cmd, *frc.frameData, frc.frameData->camera, m_lastGpuCullDrawListCount));
    if (frc.cmd) frc.cmd->SetRenderTarget(nullptr);
}

void RenderPipeline::GraphExecuteCopyDepthOcclusionDeferred() {
    const FrameRenderContext& frc = m_frameGraphExecutionContext;
    if (m_prevFrameDepthRT && m_gbuffer && m_gbuffer->GetRenderTargetRHI() && frc.device)
        frc.device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_prevFrameDepthRT);
    m_havePrevDepth = (m_prevFrameDepthRT != nullptr);
}

void RenderPipeline::GraphExecuteDeferredLighting() {
    const FrameRenderContext& frc = m_frameGraphExecutionContext;
    if (!frc.frameData) return;
    DeferredLightingPassParams p =
        BuildDeferredLightingPassParams(frc, frc.frameData->camera, { 0.f, 0.f, 0.f, 1.f });
    ExecuteDeferredLightingPass(p);
}

void RenderPipeline::GraphExecuteTransparency() {
    const FrameRenderContext& frc = m_frameGraphExecutionContext;
    if (!frc.frameData) return;
    if (m_mode == PipelineMode::Hybrid && frc.cmd)
        PrepareHybridTransparencyLighting(frc.cmd, *frc.frameData);
    TransparencyPassParams tr = BuildTransparencyPassParams(frc, frc.frameData->camera);
    ExecuteTransparencyPass(tr);
}

void RenderPipeline::GraphExecuteSkybox(bool useGbufferDepth) {
    const FrameRenderContext& frc = m_frameGraphExecutionContext;
    if (!frc.frameData) return;
    const IRenderSceneView* view = m_currentView;
    if (!view) return;
    RenderSkybox(*view, useGbufferDepth, nullptr);
}

void RenderPipeline::GraphExecuteForwardOpaque() {
    const FrameRenderContext& frc = m_frameGraphExecutionContext;
    if (!frc.frameData) return;
    if (!m_forwardOpaquePassSystem) m_forwardOpaquePassSystem = std::make_unique<ForwardOpaquePassSystem>();
    ForwardOpaquePassBuildParams build{};
    build.cmd = frc.cmd;
    build.hdrRenderTarget = m_hdrRenderTarget;
    build.width = frc.width;
    build.height = frc.height;
    build.clearColor = { 0.f, 0.f, 0.f, 1.f };
    build.setDepthTest = true;
    build.setCullFace = false;
    build.frameData = frc.frameData;
    build.camView = frc.frameData->camera;
    build.opaqueDrawables = &frc.frameData->visibleOpaque;
    build.drawCallback = [this](rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& camView,
                                const std::vector<Drawable>& opaqueDrawables) {
        RenderForwardOpaqueForCamera(cmd, frameData, camView, opaqueDrawables);
    };
    ForwardOpaquePassExecuteParams execParams = m_forwardOpaquePassSystem->BuildParams(build);
    (void)m_forwardOpaquePassSystem->Execute(execParams);
    if (frc.cmd) frc.cmd->SetRenderTarget(nullptr);
}

void RenderPipeline::GraphExecuteCopyDepthOcclusionForward() {
    const FrameRenderContext& frc = m_frameGraphExecutionContext;
    if (m_prevFrameDepthRT && m_hdrRenderTarget && frc.device)
        frc.device->BlitDepthOnly(m_hdrRenderTarget, m_prevFrameDepthRT);
    m_havePrevDepth = (m_prevFrameDepthRT != nullptr);
}

void RenderPipeline::GraphExecuteForwardPlusOpaque() {
    const FrameRenderContext& frc = m_frameGraphExecutionContext;
    if (!frc.frameData) return;
    PrepareForwardPlusFrame(frc.cmd, *frc.frameData);
    RenderForwardPlusOpaqueBatched(frc.cmd, *frc.frameData);
    if (frc.cmd) frc.cmd->SetRenderTarget(nullptr);
}

namespace {

FrameGraph::ExecuteFunc NoOpExecutor() {
    return [](const FGPassResources&) {};
}

}  // namespace

FrameGraph::ExecuteFunc RenderPipeline::ExecutorFor(FrameGraphExecutorSet set, const std::string& name) {
    if (name == FrameGraphPassNames::kGpuCull) {
        if (set == FrameGraphExecutorSet::Deferred || set == FrameGraphExecutorSet::Forward)
            return [this](const FGPassResources&) { GraphExecuteGpuCull(); };
        return NoOpExecutor();
    }
    if (name == FrameGraphPassNames::kShadow) {
        if (set == FrameGraphExecutorSet::ForwardStereo)
            return [this](const FGPassResources&) {
                const FrameRenderContext& frc = m_frameGraphExecutionContext;
                if (!frc.frameData) return;
                if (!m_shadowPassSystem) m_shadowPassSystem = std::make_unique<ShadowPassSystem>();
                m_shadowPassSystem->RenderShadowBatched(BuildShadowBatchedParams(frc.cmd, *frc.frameData, m_currentLeftCamRaw));
            };
        return [this](const FGPassResources&) { GraphExecuteShadow(); };
    }
    if (name == FrameGraphPassNames::kGBuffer) {
        if (set != FrameGraphExecutorSet::Deferred) return NoOpExecutor();
        return [this](const FGPassResources&) { GraphExecuteGBufferDeferred(); };
    }
    if (name == FrameGraphPassNames::kCopyDepthOcclusion) {
        if (set == FrameGraphExecutorSet::Deferred)
            return [this](const FGPassResources&) { GraphExecuteCopyDepthOcclusionDeferred(); };
        if (set == FrameGraphExecutorSet::Forward)
            return [this](const FGPassResources&) { GraphExecuteCopyDepthOcclusionForward(); };
        return NoOpExecutor();
    }
    if (name == FrameGraphPassNames::kLighting) {
        if (set != FrameGraphExecutorSet::Deferred) return NoOpExecutor();
        return [this](const FGPassResources&) { GraphExecuteDeferredLighting(); };
    }
    if (name == FrameGraphPassNames::kTransparency) {
        if (set == FrameGraphExecutorSet::ForwardStereo)
            return [this](const FGPassResources&) {
                const FrameRenderContext& frc = m_frameGraphExecutionContext;
                if (!frc.frameData) return;
                RenderContext c = MakeRenderContext(frc);
                if (frc.cmd && m_hdrRenderTarget) {
                    frc.cmd->SetRenderTarget(m_hdrRenderTarget);
                    const uint32_t halfW = m_width / 2;
                    frc.cmd->SetViewport(0, 0, halfW, m_height);
                }
                if (backend::PreferVulkanTransparencyPath(m_device, m_shaderCache))
                    RenderForwardTransparent(frc.cmd, *frc.frameData, m_currentLeftCam);
                else
                    TransparencyPass::Render(c, frc.frameData->visibleTransparent, m_currentLeftCam.view, m_currentLeftCam.projection,
                        m_currentLeftCam.viewPos, m_shaderCache);
                if (frc.cmd && m_hdrRenderTarget) {
                    const uint32_t halfW = m_width / 2;
                    frc.cmd->SetViewport(halfW, 0, halfW, m_height);
                }
                if (backend::PreferVulkanTransparencyPath(m_device, m_shaderCache))
                    RenderForwardTransparent(frc.cmd, *frc.frameData, m_currentRightCam);
                else
                    TransparencyPass::Render(c, frc.frameData->visibleTransparent, m_currentRightCam.view, m_currentRightCam.projection,
                        m_currentRightCam.viewPos, m_shaderCache);
                if (frc.cmd) frc.cmd->SetRenderTarget(nullptr);
            };
        if (set == FrameGraphExecutorSet::Deferred || set == FrameGraphExecutorSet::Forward || set == FrameGraphExecutorSet::ForwardPlus)
            return [this](const FGPassResources&) { GraphExecuteTransparency(); };
        return NoOpExecutor();
    }
    if (name == FrameGraphPassNames::kSkybox) {
        if (set == FrameGraphExecutorSet::Deferred)
            return [this](const FGPassResources&) { GraphExecuteSkybox(true); };
        if (set == FrameGraphExecutorSet::Forward || set == FrameGraphExecutorSet::ForwardPlus)
            return [this](const FGPassResources&) { GraphExecuteSkybox(false); };
        return NoOpExecutor();
    }
    if (name == FrameGraphPassNames::kForwardOpaque) {
        if (set == FrameGraphExecutorSet::ForwardStereo)
            return [this](const FGPassResources&) {
                const FrameRenderContext& frc = m_frameGraphExecutionContext;
                if (!frc.frameData) return;
                IBL* iblPtr = EnsureIBLFromFrameData(*frc.frameData);
                const rhi::ClearValue clearFwd = { 0.f, 0.f, 0.f, 1.f };
                if (frc.cmd && m_hdrRenderTarget)
                    rhi::ClearAndSetRenderTargetWithViewport(frc.cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearFwd);
                if (frc.cmd) {
                    frc.cmd->SetDepthTest(true);
                    frc.cmd->SetCullFace(false);
                }
                RenderForwardOpaqueStereo(frc.cmd, *frc.frameData, m_currentLeftCam, m_currentRightCam);
                (void)iblPtr;
            };
        if (set == FrameGraphExecutorSet::Forward)
            return [this](const FGPassResources&) { GraphExecuteForwardOpaque(); };
        return NoOpExecutor();
    }
    if (name == FrameGraphPassNames::kForwardPlusOpaque) {
        if (set != FrameGraphExecutorSet::ForwardPlus) return NoOpExecutor();
        return [this](const FGPassResources&) { GraphExecuteForwardPlusOpaque(); };
    }
    return NoOpExecutor();
}

FrameGraph::ExecuteFunc RenderPipeline::ExecutorForForwardStereoSkyboxLeft() {
    return [this](const FGPassResources&) {
        if (!m_currentFrameData) return;
        rhi::ICommandList* cmd = m_frameGraphExecutionContext.cmd;
        if (cmd && m_hdrRenderTarget) {
            cmd->SetRenderTarget(m_hdrRenderTarget);
            cmd->SetViewport(0, 0, m_width / 2, m_height);
        }
        auto skybox = m_currentFrameData->skybox;
        if (skybox && skybox->IsValid())
            RenderSkyboxImpl(m_currentLeftCam, skybox, false);
        else
            RenderProceduralSkyboxImpl(m_currentLeftCam, false);
    };
}

FrameGraph::ExecuteFunc RenderPipeline::ExecutorForForwardStereoSkyboxRight() {
    return [this](const FGPassResources&) {
        if (!m_currentFrameData) return;
        rhi::ICommandList* cmd = m_frameGraphExecutionContext.cmd;
        if (cmd && m_hdrRenderTarget) {
            cmd->SetRenderTarget(m_hdrRenderTarget);
            cmd->SetViewport(m_width / 2, 0, m_width / 2, m_height);
        }
        auto skybox = m_currentFrameData->skybox;
        if (skybox && skybox->IsValid())
            RenderSkyboxImpl(m_currentRightCam, skybox, false);
        else
            RenderProceduralSkyboxImpl(m_currentRightCam, false);
    };
}

FrameGraph::ExecuteFunc RenderPipeline::ResolveFrameGraphExecutor(FrameGraphExecutorSet set, const std::string& name) {
    auto itSet = m_frameGraphExecutorOverrides.find(set);
    if (itSet != m_frameGraphExecutorOverrides.end()) {
        auto it = itSet->second.find(name);
        if (it != itSet->second.end())
            return it->second;
    }
    return ExecutorFor(set, name);
}

FrameGraph::ExecuteFunc RenderPipeline::ResolveForwardStereoSkyboxExecutor(bool rightEye) {
    const char* key = rightEye ? FrameGraphPassNames::kSkyboxRight : FrameGraphPassNames::kSkyboxLeft;
    auto itSet = m_frameGraphExecutorOverrides.find(FrameGraphExecutorSet::ForwardStereo);
    if (itSet != m_frameGraphExecutorOverrides.end()) {
        auto it = itSet->second.find(key);
        if (it != itSet->second.end())
            return it->second;
    }
    return rightEye ? ExecutorForForwardStereoSkyboxRight() : ExecutorForForwardStereoSkyboxLeft();
}

void RenderPipeline::SetFrameGraphExecutorOverrides(FrameGraphExecutorSet set,
                                                    std::unordered_map<std::string, FrameGraph::ExecuteFunc> overrides) {
    m_frameGraphExecutorOverrides[set] = std::move(overrides);
}

void RenderPipeline::ClearFrameGraphExecutorOverrides(FrameGraphExecutorSet set) {
    m_frameGraphExecutorOverrides.erase(set);
}

void RenderPipeline::ClearFrameGraphExecutorOverrides() {
    m_frameGraphExecutorOverrides.clear();
}

void RenderPipeline::BuildDeferredGraph(FrameGraph& fg) {
    if (!m_gbuffer || !m_cascadeShadowMap) return;
    DeferredFrameGraphWiringInput in{};
    in.width = m_width;
    in.height = m_height;
    in.hdrColor = m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
    in.hdrDepth = m_hdrRenderTarget ? m_hdrRenderTarget->GetDepthTexture() : nullptr;
    in.gbufferPos = m_gbuffer->GetPositionTextureRHI();
    in.gbufferNorm = m_gbuffer->GetNormalTextureRHI();
    in.gbufferAlb = m_gbuffer->GetAlbedoTextureRHI();
    in.gbufferPbr = m_gbuffer->GetPBRParamsTextureRHI();
    in.gbufferVelocity = m_gbuffer->GetVelocityTextureRHI();
    in.gbufferDepth = m_gbuffer->GetDepthTextureRHI();
    in.shadowDepth = m_cascadeShadowMap->GetDepthTextureRHI();
    in.shadowWidth = m_cascadeShadowMap->GetResolution();
    in.shadowHeight = m_cascadeShadowMap->GetResolution();

    auto getExecutor = [this](const std::string& name) {
        return ResolveFrameGraphExecutor(FrameGraphExecutorSet::Deferred, name);
    };
    WireDeferredFrameGraph(fg, in, getExecutor);
}

void RenderPipeline::BuildForwardGraph(FrameGraph& fg) {
    if (!m_cascadeShadowMap || !m_hdrRenderTarget) return;
    ForwardFrameGraphWiringInput in{};
    in.width = m_width;
    in.height = m_height;
    in.hdrColor = m_hdrRenderTarget->GetColorTexture(0);
    in.shadowDepth = m_cascadeShadowMap->GetDepthTextureRHI();
    in.shadowWidth = m_cascadeShadowMap->GetResolution();
    in.shadowHeight = m_cascadeShadowMap->GetResolution();
    const bool includeGpuCull = m_gpuCullPassSystem && m_gpuCullPassSystem->IsSupported(m_device);

    auto getExecutor = [this](const std::string& name) {
        return ResolveFrameGraphExecutor(FrameGraphExecutorSet::Forward, name);
    };
    WireForwardFrameGraph(fg, in, includeGpuCull, getExecutor);
}

void RenderPipeline::BuildForwardPlusGraph(FrameGraph& fg) {
    if (!m_cascadeShadowMap || !m_hdrRenderTarget) return;
    ForwardFrameGraphWiringInput in{};
    in.width = m_width;
    in.height = m_height;
    in.hdrColor = m_hdrRenderTarget->GetColorTexture(0);
    in.shadowDepth = m_cascadeShadowMap->GetDepthTextureRHI();
    in.shadowWidth = m_cascadeShadowMap->GetResolution();
    in.shadowHeight = m_cascadeShadowMap->GetResolution();

    auto getExecutor = [this](const std::string& name) {
        return ResolveFrameGraphExecutor(FrameGraphExecutorSet::ForwardPlus, name);
    };
    WireForwardPlusFrameGraph(fg, in, getExecutor);
}

void RenderPipeline::BuildForwardStereoGraph(FrameGraph& fg) {
    if (!m_cascadeShadowMap || !m_hdrRenderTarget) return;
    ForwardFrameGraphWiringInput in{};
    in.width = m_width;
    in.height = m_height;
    in.hdrColor = m_hdrRenderTarget->GetColorTexture(0);
    in.shadowDepth = m_cascadeShadowMap->GetDepthTextureRHI();
    in.shadowWidth = m_cascadeShadowMap->GetResolution();
    in.shadowHeight = m_cascadeShadowMap->GetResolution();

    auto getExecutor = [this](const std::string& name) {
        return ResolveFrameGraphExecutor(FrameGraphExecutorSet::ForwardStereo, name);
    };
    WireForwardStereoFrameGraph(fg, in, getExecutor, ResolveForwardStereoSkyboxExecutor(false), ResolveForwardStereoSkyboxExecutor(true));
}

}  // namespace render
}  // namespace eyelib
