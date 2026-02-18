#include "eyelib/render/RenderPipeline.h"
#include "eyelib/render/GBuffer.h"
#include "eyelib/render/GpuProfilerRHI.h"
#include "eyelib/render/ShadowMap.h"
#include "eyelib/render/ClusteredLightGrid.h"
#include "eyelib/render/PostProcessor.h"
#include "eyelib/render/passes/GBufferPass.h"
#include "eyelib/render/passes/ShadowPass.h"
#include "eyelib/render/passes/LightingPass.h"
#include "eyelib/render/passes/TransparencyPass.h"
#include "eyelib/render/passes/GpuCullPass.h"
#include "eyelib/render/passes/LightCullPass.h"
#include "eyelib/render/DebugDrawRenderer.h"
#include "eyelib/debug/Presets.h"
#include "eyelib/debug/Profiler.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/TextureCube.h"
#include "eyelib/core/Log.h"
#include "eyelib/core/Time.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/rhi/RHIContext.h"
#include "eyelib/render/rhi/RHIUtils.h"

#include "eyelib/math/TransformUtils.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <glm/gtc/matrix_inverse.hpp>
#include <span>
#include <string>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;
}

RenderPipeline::RenderPipeline() = default;

RenderPipeline::~RenderPipeline() {
    Shutdown();
}

void RenderPipeline::Shutdown() {
    debug::Profiler::SetGpuProfiler(nullptr);
    m_gpuProfilerRHI.reset();

    DestroyHDRFramebuffer();
    // Destroy all objects that hold a raw pointer to m_device before destroying the device.
    // Their destructors call back into the device (DestroyRenderTarget, DestroyPipeline, etc.).
    m_postProcessor.reset();
    m_forwardPlusShader.reset();
    m_forwardPlusSkinnedShader.reset();
    m_clusteredLightGrid.reset();
    m_frameGraph.reset();
    m_lightingPass.reset();
    m_shadowPass.reset();
    m_gpuDrivenQuad.reset();
    m_lightCullPass.reset();
    m_gpuCullPass.reset();
    m_gbufferPass.reset();
    m_shadowMap.reset();
    m_gbuffer.reset();
    if (m_device) {
        if (m_bindlessHeap) { m_device->DestroyBindlessHeap(m_bindlessHeap); m_bindlessHeap = nullptr; }
        m_bindlessHandleCache.clear();
        if (m_gpuCullTransformBuffer) { m_device->DestroyBuffer(m_gpuCullTransformBuffer); m_gpuCullTransformBuffer = nullptr; }
        if (m_prevFrameDepthRT) { m_device->DestroyRenderTarget(m_prevFrameDepthRT); m_prevFrameDepthRT = nullptr; }
        if (m_pointLightBuffer) { m_device->DestroyBuffer(m_pointLightBuffer); m_pointLightBuffer = nullptr; }
        if (m_spotLightBuffer) { m_device->DestroyBuffer(m_spotLightBuffer); m_spotLightBuffer = nullptr; }
        if (m_lightIndexBuffer) { m_device->DestroyBuffer(m_lightIndexBuffer); m_lightIndexBuffer = nullptr; }
    }
    rhi::SetDefaultDevice(nullptr);
    m_device.reset();
    m_width = m_height = 0;
}

bool RenderPipeline::Initialize(std::unique_ptr<rhi::IDevice> device, uint32_t width, uint32_t height) {
    if (!device) {
        core::Log::Error("RenderPipeline::Initialize: device is null");
        return false;
    }
    m_width = width;
    m_height = height;
    m_device = std::move(device);
    rhi::SetDefaultDevice(m_device.get());

    core::Log::Info("RenderPipeline::Initialize: initializing GBuffer");
    m_gbuffer = std::make_unique<GBuffer>();
    if (!m_gbuffer->Initialize(m_device.get(), width, height)) {
        core::Log::Error("RenderPipeline::Initialize: GBuffer::Initialize failed");
        return false;
    }

    core::Log::Info("RenderPipeline::Initialize: initializing ShadowMap");
    m_shadowMap = std::make_unique<ShadowMap>();
    if (!m_shadowMap->Initialize(m_device.get(), 2048)) {
        core::Log::Error("RenderPipeline::Initialize: ShadowMap::Initialize failed");
        return false;
    }

    m_gbufferPass = std::make_unique<GBufferPass>();
    if (m_device->GetCapabilities().supportsBindless)
        m_bindlessHeap = m_device->CreateBindlessHeap();
    if (m_bindlessHeap)
        m_gbufferPass->SetBindlessHeap(m_bindlessHeap);
    m_gbufferPass->SetBindlessGetHandle([this](rhi::ITexture* t) { return GetOrAllocateBindlessHandle(t); });
    m_gpuCullPass = std::make_unique<GpuCullPass>();
    m_gpuCullPass->SetDevice(m_device.get());
    if (m_gpuCullPass->IsSupported())
        m_gpuCullPass->Initialize();
    m_lightCullPass = std::make_unique<LightCullPass>();
    m_lightCullPass->SetDevice(m_device.get());
    if (m_lightCullPass->IsSupported())
        m_lightCullPass->Initialize();
    // A2: Transform buffer for GPU cull path; draw list is now inside GpuCullPass combined buffer.
    constexpr uint32_t kMaxGpuCullDraws = 256;
    if (m_device && m_gpuCullPass->IsSupported()) {
        rhi::BufferDesc bd;
        bd.sizeInBytes = kMaxGpuCullDraws * sizeof(math::Mat4);
        bd.usage = rhi::BufferUsage::Storage;
        bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_gpuCullTransformBuffer = m_device->CreateBuffer(bd, nullptr);
    }
    // Quad for GPU-driven GBuffer path (E8): matches cull_indirect.comp output (6 indices, 1 draw).
    m_gpuDrivenQuad = std::make_unique<Mesh>();
    {
        std::vector<Vertex> verts(4);
        verts[0].position = math::Vec3(-1.0f, -1.0f, 0.0f);
        verts[0].uv = math::Vec2(0.0f, 0.0f);
        verts[1].position = math::Vec3(1.0f, -1.0f, 0.0f);
        verts[1].uv = math::Vec2(1.0f, 0.0f);
        verts[2].position = math::Vec3(1.0f, 1.0f, 0.0f);
        verts[2].uv = math::Vec2(1.0f, 1.0f);
        verts[3].position = math::Vec3(-1.0f, 1.0f, 0.0f);
        verts[3].uv = math::Vec2(0.0f, 1.0f);
        std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
        m_gpuDrivenQuad->SetVertices(verts);
        m_gpuDrivenQuad->SetIndices(indices);
        m_gpuDrivenQuad->UploadToGPU(m_device.get());
    }
    m_shadowPass = std::make_unique<ShadowPass>();
    m_lightingPass = std::make_unique<LightingPass>();
    m_gbufferPass->SetDevice(m_device.get());
    m_shadowPass->SetDevice(m_device.get());
    m_lightingPass->SetDevice(m_device.get());
    m_frameGraph = std::make_unique<FrameGraph>();
    m_frameGraph->SetDevice(m_device.get());
    m_frameGraph->SetFrameSize(width, height);
    m_clusteredLightGrid = std::make_unique<ClusteredLightGrid>();
    m_clusteredLightGrid->Initialize(width, height, 0.1f, 1000.0f);  // Default, will be updated per-frame

    core::Log::Info("RenderPipeline::Initialize: initializing PostProcessor");
    m_postProcessor = std::make_unique<PostProcessor>();
    if (!m_postProcessor->Initialize(width, height, m_device.get())) {
        core::Log::Error("RenderPipeline::Initialize: PostProcessor::Initialize failed");
        return false;
    }

    core::Log::Info("RenderPipeline::Initialize: creating HDR framebuffer");
    CreateHDRFramebuffer();
    if (m_hdrRenderTarget == nullptr) {
        core::Log::Error("RenderPipeline::Initialize: CreateHDRFramebuffer failed (m_hdrRenderTarget is null)");
        return false;
    }
    if (width > 0 && height > 0) {
        std::vector<rhi::RHITextureFormat> noColor;
        m_prevFrameDepthRT = m_device->CreateRenderTarget(width, height, noColor, rhi::RHITextureFormat::Depth24, 1);
    }

    // Initialize previous matrices (identity for first frame)
    m_prevView = math::Mat4(1.0f);
    m_prevProjection = math::Mat4(1.0f);

    m_gpuProfilerRHI = std::make_unique<GpuProfilerRHI>(m_device.get());
    debug::Profiler::SetGpuProfiler(m_gpuProfilerRHI.get());

    core::Log::Info("RenderPipeline::Initialize: pipeline init success");
    return true;
}

void RenderPipeline::Resize(uint32_t width, uint32_t height) {
#ifndef NDEBUG
    assert(!m_rendering && "Resize may only be called at frame boundary, before Render().");
#endif
    if (width == 0 || height == 0) return;
    m_windowWidth = width;
    m_windowHeight = height;
    // Fixed aspect (16:9) so the scene never stretches. Cover: render size chosen so the visible crop fills the window.
    const float aspectW = 16.0f;
    const float aspectH = 9.0f;
    float scale = static_cast<float>(width) / aspectW;
    float scaleH = static_cast<float>(height) / aspectH;
    if (scaleH > scale) scale = scaleH;
    uint32_t renderW = static_cast<uint32_t>(aspectW * scale);
    uint32_t renderH = static_cast<uint32_t>(aspectH * scale);
    if (renderW == 0) renderW = 1;
    if (renderH == 0) renderH = 1;
    if (renderW == m_width && renderH == m_height) return;
    m_width = renderW;
    m_height = renderH;
    if (m_gbuffer) m_gbuffer->Resize(renderW, renderH);
    if (m_prevFrameDepthRT) {
        m_device->DestroyRenderTarget(m_prevFrameDepthRT);
        m_prevFrameDepthRT = nullptr;
        m_havePrevDepth = false;
    }
    if (m_device && renderW > 0 && renderH > 0) {
        std::vector<rhi::RHITextureFormat> noColor;
        m_prevFrameDepthRT = m_device->CreateRenderTarget(renderW, renderH, noColor, rhi::RHITextureFormat::Depth24, 1);
    }
    DestroyHDRFramebuffer();
    CreateHDRFramebuffer();
    if (m_frameGraph) m_frameGraph->SetFrameSize(renderW, renderH);
    if (m_clusteredLightGrid) {
        m_clusteredLightGrid->Initialize(renderW, renderH, 0.1f, 1000.0f);  // Will be updated per-frame with actual camera
    }
    if (m_postProcessor) {
        m_postProcessor->Resize(renderW, renderH);
    }
    rhi::ICommandList* cmd = m_device ? m_device->GetImmediateCommandList() : nullptr;
    if (cmd) cmd->SetViewport(0, 0, renderW, renderH);
}

void RenderPipeline::SetFrameSize(unsigned w, unsigned h) {
    Resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
}

void RenderPipeline::EnsureResources(uint32_t width, uint32_t height) {
    if (!m_gbuffer || !m_device) return;
    Resize(width, height);
}

void RenderPipeline::CreateHDRFramebuffer() {
    if (m_width == 0 || m_height == 0 || !m_device) return;
    DestroyHDRFramebuffer();
    std::vector<rhi::RHITextureFormat> colorFormats = { rhi::RHITextureFormat::RGBA16F };
    m_hdrRenderTarget = m_device->CreateRenderTarget(
        static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height),
        colorFormats, rhi::RHITextureFormat::Depth24, 1);
}

void RenderPipeline::DestroyHDRFramebuffer() {
    // Caller must ensure this is only called at frame boundary (e.g. from Resize), not while a frame is in flight.
    if (m_device && m_hdrRenderTarget) {
        m_device->DestroyRenderTarget(m_hdrRenderTarget);
        m_hdrRenderTarget = nullptr;
    }
}

rhi::ITexture* RenderPipeline::GetHDRColorTextureRHI() const {
    return m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
}

rhi::BindlessHandle RenderPipeline::GetOrAllocateBindlessHandle(rhi::ITexture* texture) {
    if (!texture || !m_bindlessHeap || !m_device->GetCapabilities().supportsBindless)
        return rhi::kInvalidBindlessHandle;
    auto it = m_bindlessHandleCache.find(texture);
    if (it != m_bindlessHandleCache.end())
        return it->second;
    rhi::BindlessHandle h = m_bindlessHeap->AllocateTextureHandle(texture);
    if (h != rhi::kInvalidBindlessHandle)
        m_bindlessHandleCache[texture] = h;
    return h;
}

uint32_t RenderPipeline::PrepareGpuCullFromView(const IRenderSceneView& view, const CameraView& cam) {
    constexpr uint32_t kMaxGpuCullDraws = 256;
    m_scratchGpuCullDrawList.clear();
    m_scratchGpuCullTransforms.clear();
    m_gpuCullSingleMesh = true;
    m_gpuCullRepresentativeMesh.reset();
    Mesh* firstMeshPtr = nullptr;
    view.ForEachOpaqueDrawable([this, &firstMeshPtr](const Drawable& d) {
        if (!d.mesh || !d.mesh->IsValid() || m_scratchGpuCullDrawList.size() >= kMaxGpuCullDraws) return;
        if (!firstMeshPtr) {
            firstMeshPtr = d.mesh.get();
            m_gpuCullRepresentativeMesh = d.mesh;
        } else if (d.mesh.get() != firstMeshPtr) {
            m_gpuCullSingleMesh = false;
        }
        math::AABB localAABB = d.mesh->GetAABB();
        if (!localAABB.IsValid()) return;
        math::AABB worldAABB = localAABB.Transform(d.transform);
        GpuCullDrawEntry e = {};
        e.aabbMin[0] = worldAABB.min.x; e.aabbMin[1] = worldAABB.min.y; e.aabbMin[2] = worldAABB.min.z;
        e.aabbMax[0] = worldAABB.max.x; e.aabbMax[1] = worldAABB.max.y; e.aabbMax[2] = worldAABB.max.z;
        const size_t indexCount = d.mesh->GetIndexCount();
        e.indexCount = static_cast<uint32_t>(indexCount);
        e.instanceCount = 1u;
        e.firstIndex = 0u;
        e.baseVertex = 0;
        e.baseInstance = 0u;
        m_scratchGpuCullDrawList.push_back(e);
        m_scratchGpuCullTransforms.push_back(d.transform);
    });
    if (m_scratchGpuCullDrawList.empty()) m_gpuCullSingleMesh = false;
    const uint32_t gpuCullDrawListCount = static_cast<uint32_t>(m_scratchGpuCullDrawList.size());

    if (m_gpuCullPass && m_gpuCullPass->IsSupported()) {
        m_gpuCullPass->SetDevice(m_device.get());
        if (gpuCullDrawListCount > 0) {
            m_gpuCullPass->SetDrawList(m_scratchGpuCullDrawList.data(), gpuCullDrawListCount);
            if (m_gpuCullTransformBuffer && m_scratchGpuCullTransforms.size() >= gpuCullDrawListCount) {
                const size_t transformSize = gpuCullDrawListCount * sizeof(math::Mat4);
                m_gpuCullTransformBuffer->Update(m_scratchGpuCullTransforms.data(), 0, transformSize);
            }
            const math::Mat4 viewProj = cam.projection * cam.view;
            m_gpuCullPass->SetViewProj(&viewProj[0][0]);
        } else {
            m_gpuCullPass->SetDrawList(nullptr, 0);
        }
        if (m_havePrevDepth && m_prevFrameDepthRT && m_prevFrameDepthRT->GetDepthTexture())
            m_gpuCullPass->SetOcclusionDepth(m_prevFrameDepthRT->GetDepthTexture(), m_width, m_height);
        else
            m_gpuCullPass->SetOcclusionDepth(nullptr, 0, 0);
    }
    return gpuCullDrawListCount;
}

void RenderPipeline::RenderOpaqueGeometryToGBuffer(rhi::ICommandList* cmd, const IRenderSceneView& view, const CameraView& cam, uint32_t gpuCullDrawListCount) {
    if (!m_gbufferPass || !m_gbuffer) return;
    const bool useIndirectQuad = m_useGpuDrivenGBuffer && m_gpuCullPass && m_gpuCullPass->IsSupported()
        && m_gpuDrivenQuad && m_gpuDrivenQuad->IsValid() && (gpuCullDrawListCount == 0);
    const bool useIndirectScene = m_useGpuDrivenGBuffer && m_gpuCullPass && m_gpuCullPass->IsSupported()
        && gpuCullDrawListCount > 0 && m_gpuCullSingleMesh && m_gpuCullRepresentativeMesh && m_gpuCullRepresentativeMesh->IsValid()
        && m_gpuCullTransformBuffer;
    if (useIndirectQuad) {
        m_gbufferPass->RenderIndirect(*m_gbuffer, cmd,
            m_gpuCullPass->GetIndirectBuffer(),
            m_gpuCullPass->GetDrawCount(),
            m_gpuCullPass->GetIndirectStride(),
            m_gpuDrivenQuad.get(),
            cam.view, cam.projection, &m_prevView, &m_prevProjection);
    } else if (useIndirectScene) {
        m_gbufferPass->RenderIndirectWithPerDrawData(*m_gbuffer, cmd,
            m_gpuCullPass->GetIndirectBuffer(),
            m_gpuCullPass->GetDrawCount(),
            m_gpuCullPass->GetIndirectStride(),
            m_gpuCullRepresentativeMesh.get(),
            m_gpuCullTransformBuffer,
            gpuCullDrawListCount,
            cam.view, cam.projection, &m_prevView, &m_prevProjection);
    } else {
        m_gbufferPass->Render(*m_gbuffer, cmd,
            [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
            cam.view, cam.projection, m_prevView, m_prevProjection);
    }
}

void RenderPipeline::BuildAndExecuteFrameGraph(const IRenderSceneView& view) {
    if (!m_frameGraph || !m_gbuffer || !m_shadowMap || !m_gbufferPass || !m_shadowPass || !m_lightingPass) return;
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);

    const CameraView cam = view.GetCamera();
    const LightParams light = view.GetLight();

    // Fill scratch lights for lighting pass (view may be API or engine)
    m_scratchPointLights.clear();
    m_scratchSpotLights.clear();
    view.ForEachPointLight([this](const PointLight& p) { m_scratchPointLights.push_back(p); });
    view.ForEachSpotLight([this](const SpotLight& s) { m_scratchSpotLights.push_back(s); });

    rhi::ITexture* hdrColorTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
    rhi::ITexture* hdrDepthTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetDepthTexture() : nullptr;
    auto hdrColor = m_frameGraph->ImportTextureRHI("HDR", hdrColorTex, m_width, m_height);
    auto hdrDepth = m_frameGraph->ImportTextureRHI("HDRDepth", hdrDepthTex, m_width, m_height);
    auto gbPos = m_frameGraph->ImportTextureRHI("GBufferPos", m_gbuffer->GetPositionTextureRHI(), m_width, m_height);
    auto gbNorm = m_frameGraph->ImportTextureRHI("GBufferNorm", m_gbuffer->GetNormalTextureRHI(), m_width, m_height);
    auto gbAlb = m_frameGraph->ImportTextureRHI("GBufferAlb", m_gbuffer->GetAlbedoTextureRHI(), m_width, m_height);
    auto gbPBR = m_frameGraph->ImportTextureRHI("GBufferPBR", m_gbuffer->GetPBRParamsTextureRHI(), m_width, m_height);
    auto gbVelocity = m_frameGraph->ImportTextureRHI("GBufferVelocity", m_gbuffer->GetVelocityTextureRHI(), m_width, m_height);
    auto gbDepth = m_frameGraph->ImportTextureRHI("GBufferDepth", m_gbuffer->GetDepthTextureRHI(), m_width, m_height);
    auto shadowTex = m_frameGraph->ImportTextureRHI("Shadow", m_shadowMap->GetDepthTextureRHI(), m_shadowMap->GetResolution(), m_shadowMap->GetResolution());
    rhi::ICommandList* cmd = GetRHICommandList();

    const uint32_t gpuCullDrawListCount = PrepareGpuCullFromView(view, cam);
    if (m_gpuCullPass && m_gpuCullPass->IsSupported()) {
        m_frameGraph->AddPass("GPU Cull",
            [this, cmd](const FGPassResources&) {
                if (m_gpuCullPass) m_gpuCullPass->Execute(cmd);
            },
            {}, {}, true);
    }

    if (light.castShadows) {
        m_frameGraph->AddPass("Shadow",
            [this, &view, &light, &cam, cmd](const FGPassResources&) {
                m_shadowPass->Render(*m_shadowMap, cmd,
                    [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
                    light.direction, cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
            },
            {}, { shadowTex }, true);
    }

    m_frameGraph->AddPass("GBuffer",
        [this, &view, &cam, cmd, gpuCullDrawListCount](const FGPassResources&) {
            RenderOpaqueGeometryToGBuffer(cmd, view, cam, gpuCullDrawListCount);
        },
        {}, { gbPos, gbNorm, gbAlb, gbPBR, gbVelocity, gbDepth }, true);

    m_frameGraph->AddPass("Copy depth for occlusion",
        [this](const FGPassResources&) {
            if (m_prevFrameDepthRT && m_gbuffer && m_gbuffer->GetRenderTargetRHI())
                m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_prevFrameDepthRT);
            m_havePrevDepth = (m_prevFrameDepthRT != nullptr);
        },
        { gbDepth }, {}, true);

    std::vector<FGTextureHandle> lightingReads = { gbPos, gbNorm, gbAlb, gbPBR };
    if (light.castShadows) lightingReads.push_back(shadowTex);
    rhi::ClearValue clearBlack = { 0.f, 0.f, 0.f, 1.f };
    m_frameGraph->AddPass("Lighting",
        [this, &view, &cam, cmd, clearBlack](const FGPassResources&) {
            if (cmd && m_hdrRenderTarget)
                rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearBlack);
            ShadowMap* shadowMapForLighting = view.GetLight().castShadows ? m_shadowMap.get() : nullptr;
            m_lightingPass->Render(*m_gbuffer, cmd, view.GetLight(), cam.view, cam.projection, cam.viewPos, shadowMapForLighting,
                &m_scratchPointLights, &m_scratchSpotLights);
        },
        lightingReads, { hdrColor }, true);

    m_frameGraph->AddPass("Transparency",
        [this, &view, &cam, cmd](const FGPassResources&) {
            if (m_device && m_hdrRenderTarget)
                m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_hdrRenderTarget);
            if (cmd && m_hdrRenderTarget) {
                cmd->SetRenderTarget(m_hdrRenderTarget);
                cmd->SetViewport(0, 0, m_width, m_height);
            }
            m_scratchTransparent.clear();
            view.ForEachTransparentDrawable([this](const Drawable& d) { m_scratchTransparent.push_back(d); });
            TransparencyPass::Render(cmd, m_scratchTransparent, cam.view, cam.projection, cam.viewPos);
        },
        {}, { hdrColor }, true);

    if (m_postProcessor && m_postProcessor->GetPingPongRT(0) && m_postProcessor->GetOutputRenderTargetRHI()) {
        rhi::ITexture* postToneMappedTex = m_postProcessor->GetPingPongRT(0)->GetColorTexture(0);
        rhi::ITexture* postOutputTex = m_postProcessor->GetOutputRenderTargetRHI()->GetColorTexture(0);
        if (postToneMappedTex && postOutputTex) {
            auto postToneMapped = m_frameGraph->ImportTextureRHI("PostToneMapped", postToneMappedTex, m_width, m_height);
            auto postOutput = m_frameGraph->ImportTextureRHI("PostOutput", postOutputTex, m_width, m_height);
            PostProcessor::Settings settings;
            m_postProcessor->AddPostPassesToFrameGraph(m_frameGraph.get(), hdrColor, postToneMapped, postOutput, settings, cmd);
        }
    }

    m_frameGraph->Compile();
    m_frameGraph->Execute(GetRHICommandList());
}

void RenderPipeline::BuildAndExecuteForwardFrameGraph(const IRenderSceneView& view) {
    if (!m_frameGraph || !m_shadowMap || !m_shadowPass || !m_hdrRenderTarget) return;
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);

    const CameraView cam = view.GetCamera();
    const LightParams light = view.GetLight();

    m_scratchPointLights.clear();
    m_scratchSpotLights.clear();
    view.ForEachPointLight([this](const PointLight& p) { m_scratchPointLights.push_back(p); });
    view.ForEachSpotLight([this](const SpotLight& s) { m_scratchSpotLights.push_back(s); });

    rhi::ITexture* hdrColorTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
    auto hdrColor = m_frameGraph->ImportTextureRHI("HDR", hdrColorTex, m_width, m_height);
    auto shadowTex = m_frameGraph->ImportTextureRHI("Shadow", m_shadowMap->GetDepthTextureRHI(), m_shadowMap->GetResolution(), m_shadowMap->GetResolution());
    rhi::ICommandList* cmd = GetRHICommandList();

    const uint32_t gpuCullDrawListCount = PrepareGpuCullFromView(view, cam);
    if (m_gpuCullPass && m_gpuCullPass->IsSupported()) {
        m_frameGraph->AddPass("GPU Cull",
            [this, cmd](const FGPassResources&) {
                if (m_gpuCullPass) m_gpuCullPass->Execute(cmd);
            },
            {}, {}, true);
    }

    if (light.castShadows) {
        m_frameGraph->AddPass("Shadow",
            [this, &view, &light, &cam, cmd](const FGPassResources&) {
                m_shadowPass->Render(*m_shadowMap, cmd,
                    [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
                    light.direction, cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
            },
            {}, { shadowTex }, true);
    }

    rhi::ClearValue clearFwd = { 0.1f, 0.1f, 0.15f, 1.f };
    m_frameGraph->AddPass("Forward Opaque",
        [this, &view, cmd, clearFwd](const FGPassResources&) {
            if (cmd && m_hdrRenderTarget)
                rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearFwd);
            if (cmd) { cmd->SetDepthTest(true); cmd->SetCullFace(false); }
            RenderForwardOpaque(cmd, view);
        },
        {}, { hdrColor }, true);

    m_frameGraph->AddPass("Copy depth for occlusion",
        [this](const FGPassResources&) {
            if (m_prevFrameDepthRT && m_hdrRenderTarget)
                m_device->BlitDepthOnly(m_hdrRenderTarget, m_prevFrameDepthRT);
            m_havePrevDepth = (m_prevFrameDepthRT != nullptr);
        },
        { hdrColor }, {}, true);

    m_frameGraph->AddPass("Transparency",
        [this, &view, &cam, cmd](const FGPassResources&) {
            if (cmd && m_hdrRenderTarget) {
                cmd->SetRenderTarget(m_hdrRenderTarget);
                cmd->SetViewport(0, 0, m_width, m_height);
            }
            m_scratchTransparent.clear();
            view.ForEachTransparentDrawable([this](const Drawable& d) { m_scratchTransparent.push_back(d); });
            TransparencyPass::Render(cmd, m_scratchTransparent, cam.view, cam.projection, cam.viewPos);
        },
        {}, { hdrColor }, true);

    if (m_postProcessor && m_postProcessor->GetPingPongRT(0) && m_postProcessor->GetOutputRenderTargetRHI()) {
        rhi::ITexture* postToneMappedTex = m_postProcessor->GetPingPongRT(0)->GetColorTexture(0);
        rhi::ITexture* postOutputTex = m_postProcessor->GetOutputRenderTargetRHI()->GetColorTexture(0);
        if (postToneMappedTex && postOutputTex) {
            auto postToneMapped = m_frameGraph->ImportTextureRHI("PostToneMapped", postToneMappedTex, m_width, m_height);
            auto postOutput = m_frameGraph->ImportTextureRHI("PostOutput", postOutputTex, m_width, m_height);
            PostProcessor::Settings settings;
            m_postProcessor->AddPostPassesToFrameGraph(m_frameGraph.get(), hdrColor, postToneMapped, postOutput, settings, cmd);
        }
    }

    m_frameGraph->Compile();
    m_frameGraph->Execute(GetRHICommandList());
}

void RenderPipeline::RenderForwardOpaque(rhi::ICommandList* cmd, const IRenderSceneView& view) {
    const CameraView cam = view.GetCamera();
    const LightParams light = view.GetLight();
    const int kShadowMapUnit = static_cast<int>(rhi::PassBindings::ShadowMap);
    const bool useShadows = (m_shadowMap != nullptr && light.castShadows);
    if (useShadows && cmd) {
        rhi::ITexture* shadowTex = m_shadowMap->GetDepthTextureRHI();
        if (shadowTex) cmd->BindTexture(shadowTex, kShadowMapUnit);
    }

    const int kMaxPointLights = 8;
    const int kMaxSpotLights = 4;
    const int numPoint = static_cast<int>(std::min(m_scratchPointLights.size(), size_t(kMaxPointLights)));
    const int numSpot = static_cast<int>(std::min(m_scratchSpotLights.size(), size_t(kMaxSpotLights)));

    view.ForEachOpaqueDrawable([&](const Drawable& d) {
        if (!d.mesh || !d.material) return;
        if (d.materialInstance) {
            d.materialInstance->Bind();
        } else {
            d.material->Bind();
        }
        Shader* sh = (d.materialInstance ? d.materialInstance->GetMaterial() : d.material)->GetShader().get();
        if (!sh) return;
        sh->SetMat4("u_Model", d.transform);
        sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
        sh->SetMat4("u_View", cam.view);
        sh->SetMat4("u_Projection", cam.projection);
        sh->SetVec3("u_ViewPos", cam.viewPos);
        sh->SetVec3("u_LightDir", light.direction);
        sh->SetVec3("u_LightColor", light.color);
        sh->SetFloat("u_LightIntensity", light.intensity);
        sh->SetVec3("u_Ambient", light.ambient);
        sh->SetFloat("u_Exposure", light.exposure);
        if (useShadows) {
            sh->SetInt("u_ShadowMap", kShadowMapUnit);
            sh->SetMat4("u_LightSpaceMatrix", m_shadowMap->GetShadowMatrix());
            sh->SetFloat("u_ShadowBias", 0.005f);
            sh->SetInt("u_UseShadows", 1);
        } else {
            sh->SetInt("u_UseShadows", 0);
        }
        const math::Mat4* bonePtr = d.bones;
        size_t boneCount = d.boneCount;
        if ((!bonePtr || boneCount == 0) && d.materialInstance) {
            d.materialInstance->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                if (name == "u_Bones" && values && count > 0) { bonePtr = values; boneCount = count; }
            });
        } else if (!bonePtr || boneCount == 0) {
            d.material->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                if (name == "u_Bones" && values && count > 0) { bonePtr = values; boneCount = count; }
            });
        }
        if (bonePtr && boneCount > 0) {
            boneCount = std::min(boneCount, kMaxBones);
            sh->SetMat4Array("u_Bones", bonePtr, boneCount);
        }
        sh->SetInt("u_PointLightCount", numPoint);
        for (int i = 0; i < numPoint; ++i) {
            const auto& p = m_scratchPointLights[static_cast<size_t>(i)];
            sh->SetVec3("u_PointLightPos[" + std::to_string(i) + "]", p.position);
            sh->SetVec3("u_PointLightColor[" + std::to_string(i) + "]", p.color);
            sh->SetFloat("u_PointLightIntensity[" + std::to_string(i) + "]", p.intensity);
            sh->SetFloat("u_PointLightRadius[" + std::to_string(i) + "]", p.radius);
        }
        sh->SetInt("u_SpotLightCount", numSpot);
        for (int i = 0; i < numSpot; ++i) {
            const auto& s = m_scratchSpotLights[static_cast<size_t>(i)];
            sh->SetVec3("u_SpotLightPos[" + std::to_string(i) + "]", s.position);
            sh->SetVec3("u_SpotLightDir[" + std::to_string(i) + "]", s.direction);
            sh->SetVec3("u_SpotLightColor[" + std::to_string(i) + "]", s.color);
            sh->SetFloat("u_SpotLightIntensity[" + std::to_string(i) + "]", s.intensity);
            sh->SetFloat("u_SpotLightRange[" + std::to_string(i) + "]", s.range);
            sh->SetFloat("u_SpotLightInnerCos[" + std::to_string(i) + "]", s.innerConeCos);
            sh->SetFloat("u_SpotLightOuterCos[" + std::to_string(i) + "]", s.outerConeCos);
        }
        d.mesh->Bind();
        d.mesh->Draw();
        d.mesh->Unbind();
        sh->Unbind();
    });

    if (useShadows && cmd) cmd->UnbindTexture(kShadowMapUnit);
}

void RenderPipeline::RenderForward(const IRenderSceneView& view) {
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);
    rhi::ICommandList* cmd = GetRHICommandList();
    if (cmd && m_hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, cv);
    }
    if (cmd) { cmd->SetDepthTest(true); cmd->SetCullFace(false); }

    m_scratchPointLights.clear();
    m_scratchSpotLights.clear();
    view.ForEachPointLight([this](const PointLight& p) { m_scratchPointLights.push_back(p); });
    view.ForEachSpotLight([this](const SpotLight& s) { m_scratchSpotLights.push_back(s); });

    RenderForwardOpaque(cmd, view);

    const CameraView cam = view.GetCamera();
    m_scratchTransparent.clear();
    view.ForEachTransparentDrawable([this](const Drawable& d) { m_scratchTransparent.push_back(d); });
    TransparencyPass::Render(cmd, m_scratchTransparent, cam.view, cam.projection, cam.viewPos);
    if (cmd) { cmd->SetCullFace(true); cmd->SetRenderTarget(nullptr); }
}

void RenderPipeline::RenderForwardPlus(const IRenderSceneView& view) {
    if (!m_clusteredLightGrid) {
        RenderForward(view);
        return;
    }

    const CameraView cam = view.GetCamera();
    const LightParams light = view.GetLight();

    m_scratchPointLights.clear();
    m_scratchSpotLights.clear();
    view.ForEachPointLight([this](const PointLight& p) { m_scratchPointLights.push_back(p); });
    view.ForEachSpotLight([this](const SpotLight& s) { m_scratchSpotLights.push_back(s); });

    // Reinitialize clustered grid if camera planes changed
    if (m_clusteredLightGrid->GetNearPlane() != cam.nearPlane ||
        m_clusteredLightGrid->GetFarPlane() != cam.farPlane) {
        m_clusteredLightGrid->Initialize(m_width, m_height, cam.nearPlane, cam.farPlane);
    }

    // Upload lights to SSBOs (RHI)
    rhi::ICommandList* cmdFwd = GetRHICommandList();
    const size_t kMaxPointLights = 256;
    const size_t kMaxSpotLights = 64;
    const size_t kMaxLightIndices = 4 * 1024 * 1024;
    if (!m_scratchPointLights.empty() && m_device) {
        if (!m_pointLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxPointLights * sizeof(PointLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_pointLightBuffer = m_device->CreateBuffer(bd, nullptr);
        }
        if (m_pointLightBuffer) {
            const size_t sz = m_scratchPointLights.size() * sizeof(PointLight);
            m_pointLightBuffer->Update(m_scratchPointLights.data(), 0, sz);
        }
    }
    if (!m_scratchSpotLights.empty() && m_device) {
        if (!m_spotLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxSpotLights * sizeof(SpotLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_spotLightBuffer = m_device->CreateBuffer(bd, nullptr);
        }
        if (m_spotLightBuffer) {
            const size_t sz = m_scratchSpotLights.size() * sizeof(SpotLight);
            m_spotLightBuffer->Update(m_scratchSpotLights.data(), 0, sz);
        }
    }
    if (m_device && !m_lightIndexBuffer) {
        rhi::BufferDesc bd;
        bd.sizeInBytes = kMaxLightIndices;
        bd.usage = rhi::BufferUsage::Storage;
        bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_lightIndexBuffer = m_device->CreateBuffer(bd, nullptr);
    }

    // Light grid: GPU compute cull (when supported) or CPU fallback
    if (m_lightCullPass && m_lightCullPass->IsSupported() && m_lightIndexBuffer) {
        m_lightCullPass->SetDevice(m_device.get());
        m_lightCullPass->SetPointLights(m_pointLightBuffer, static_cast<uint32_t>(m_scratchPointLights.size()));
        m_lightCullPass->SetSpotLights(m_spotLightBuffer, static_cast<uint32_t>(m_scratchSpotLights.size()));
        m_lightCullPass->SetOutputBuffer(m_lightIndexBuffer);
        m_lightCullPass->SetViewProj(&cam.view[0][0], &cam.projection[0][0]);
        m_lightCullPass->SetParams(
            m_clusteredLightGrid->GetTileCountX(),
            m_clusteredLightGrid->GetTileCountY(),
            m_clusteredLightGrid->GetSliceCount(),
            m_clusteredLightGrid->GetLogNear(),
            m_clusteredLightGrid->GetLogFar(),
            m_clusteredLightGrid->GetNearPlane(),
            m_clusteredLightGrid->GetFarPlane(),
            m_width, m_height);
        m_lightCullPass->Execute(cmdFwd);
    } else {
        m_clusteredLightGrid->BuildLightGrid(m_scratchPointLights, m_scratchSpotLights,
                                             cam.view, cam.projection, cam.viewPos);
        m_clusteredLightGrid->UploadToGPU(m_lightIndexBuffer);
    }
    if (cmdFwd && m_lightIndexBuffer) cmdFwd->BindStorageBuffer(m_lightIndexBuffer, 2);

    if (light.castShadows && m_shadowMap && m_shadowPass) {
        m_shadowPass->Render(*m_shadowMap, GetRHICommandList(),
            [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
            light.direction, cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
    }

    // Ensure forward+ shader exists (backend-owned loading by logical name)
    if (!m_forwardPlusShader || !m_forwardPlusShader->IsValid()) {
        m_forwardPlusShader = std::make_shared<Shader>();
        if (m_device)
            m_forwardPlusShader->Load(m_device.get(), "passes/forward/forwardplus", Mesh::GetVertexLayoutRHI());
    }

    if (!m_forwardPlusShader->IsValid()) {
        RenderForward(view);
        return;
    }

    if (!m_forwardPlusSkinnedShader || !m_forwardPlusSkinnedShader->IsValid()) {
        m_forwardPlusSkinnedShader = std::make_shared<Shader>();
        if (m_device)
            m_forwardPlusSkinnedShader->Load(m_device.get(), "passes/forward/forwardplus_skinned", Mesh::GetVertexLayoutRHI());
    }

    if (cmdFwd && m_hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmdFwd, m_hdrRenderTarget, 0, 0, m_width, m_height, cv);
    }
    if (cmdFwd) { cmdFwd->SetDepthTest(true); cmdFwd->SetCullFace(false); }

    const int kShadowMapUnit = static_cast<int>(rhi::PassBindings::ShadowMap);
    if (light.castShadows && m_shadowMap && cmdFwd) {
        rhi::ITexture* shadowTex = m_shadowMap->GetDepthTextureRHI();
        if (shadowTex) cmdFwd->BindTexture(shadowTex, kShadowMapUnit);
    }

    auto setCommonForwardPlusUniforms = [&](Shader* sh) {
        sh->SetMat4("u_View", cam.view);
        sh->SetMat4("u_Projection", cam.projection);
        sh->SetVec3("u_ViewPos", cam.viewPos);
        sh->SetVec3("u_DirLightDir", light.direction);
        sh->SetVec3("u_DirLightColor", light.color);
        sh->SetFloat("u_DirLightIntensity", light.intensity);
        sh->SetVec3("u_Ambient", light.ambient);
        sh->SetFloat("u_Exposure", light.exposure);
        sh->SetInt("u_ScreenWidth", static_cast<int>(m_width));
        sh->SetInt("u_ScreenHeight", static_cast<int>(m_height));
        sh->SetInt("u_TileCountX", static_cast<int>(m_clusteredLightGrid->GetTileCountX()));
        sh->SetInt("u_TileCountY", static_cast<int>(m_clusteredLightGrid->GetTileCountY()));
        sh->SetInt("u_SliceCount", static_cast<int>(m_clusteredLightGrid->GetSliceCount()));
        sh->SetFloat("u_LogNear", m_clusteredLightGrid->GetLogNear());
        sh->SetFloat("u_LogFar", m_clusteredLightGrid->GetLogFar());
        sh->SetFloat("u_NearPlane", m_clusteredLightGrid->GetNearPlane());
        sh->SetFloat("u_FarPlane", m_clusteredLightGrid->GetFarPlane());
        sh->SetInt("u_PointLightCount", static_cast<int>(m_scratchPointLights.size()));
        sh->SetInt("u_SpotLightCount", static_cast<int>(m_scratchSpotLights.size()));
        if (light.castShadows && m_shadowMap) {
            sh->SetInt("u_ShadowMap", kShadowMapUnit);
            sh->SetMat4("u_LightSpaceMatrix", m_shadowMap->GetShadowMatrix());
            sh->SetFloat("u_ShadowBias", 0.005f);
            sh->SetInt("u_UseShadows", 1);
        } else {
            sh->SetInt("u_UseShadows", 0);
        }
    };

    const int kAlbedoTextureUnit = 0;
    const int kNormalTextureUnit = 1;
    const int kMetallicRoughnessTextureUnit = 2;
    const int kAOTextureUnit = 3;
    const int kEmissiveTextureUnit = 4;
    const math::Vec4 kDefaultUVTransform(0.0f, 0.0f, 1.0f, 1.0f);
    bool commonSetNonSkinned = false;
    bool commonSetSkinned = false;

    view.ForEachOpaqueDrawable([&](const Drawable& d) {
        if (!d.mesh) return;
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!material) return;

        const math::Mat4* bonePtr = d.bones;
        size_t boneCount = d.boneCount;
        auto collectBones = [&](const std::string& name, const math::Mat4* values, size_t count) {
            if (name == "u_Bones" && values && count > 0) { bonePtr = values; boneCount = count; }
        };
        if ((!bonePtr || boneCount == 0) && d.materialInstance) d.materialInstance->ForEachMat4Array(collectBones);
        else if (!bonePtr || boneCount == 0) material->ForEachMat4Array(collectBones);

        const bool useSkinned = (bonePtr && boneCount > 0 && m_forwardPlusSkinnedShader && m_forwardPlusSkinnedShader->IsValid());
        Shader* sh = useSkinned ? m_forwardPlusSkinnedShader.get() : m_forwardPlusShader.get();
        sh->Bind();

        if (useSkinned) {
            if (!commonSetSkinned) { setCommonForwardPlusUniforms(sh); commonSetSkinned = true; }
        } else {
            if (!commonSetNonSkinned) { setCommonForwardPlusUniforms(sh); commonSetNonSkinned = true; }
        }

        sh->SetMat4("u_Model", d.transform);
        sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
        if (useSkinned) {
            boneCount = std::min(boneCount, kMaxBones);
            sh->SetMat4Array("u_Bones", bonePtr, boneCount);
        }

        const math::Vec4 uvTransform = d.materialInstance ? d.materialInstance->GetVec4("u_UVTransform", kDefaultUVTransform) : material->GetVec4("u_UVTransform", kDefaultUVTransform);
        const float uvRotation = d.materialInstance ? d.materialInstance->GetFloat("u_UVRotation", 0.0f) : material->GetFloat("u_UVRotation", 0.0f);
        const float texCoordIndex = d.materialInstance ? d.materialInstance->GetFloat("u_TexCoordIndex", 0.0f) : material->GetFloat("u_TexCoordIndex", 0.0f);
        const float flipV = d.materialInstance ? d.materialInstance->GetFloat("u_FlipV", 0.0f) : material->GetFloat("u_FlipV", 0.0f);
        sh->SetVec4("u_UVTransform", uvTransform);
        sh->SetFloat("u_UVRotation", uvRotation);
        sh->SetFloat("u_TexCoordIndex", texCoordIndex);
        sh->SetFloat("u_FlipV", flipV);

        const math::Vec3 albedo = d.materialInstance ? d.materialInstance->GetAlbedo() : material->GetAlbedo();
        const float metallic = d.materialInstance ? d.materialInstance->GetMetallic() : material->GetMetallic();
        const float roughness = d.materialInstance ? d.materialInstance->GetRoughness() : material->GetRoughness();
        const float ao = d.materialInstance ? d.materialInstance->GetAO() : material->GetAO();
        const math::Vec3 emission = d.materialInstance ? d.materialInstance->GetEmission() : material->GetEmission();
        const float alpha = d.materialInstance ? d.materialInstance->GetAlpha() : material->GetAlpha();
        sh->SetVec3("u_Albedo", albedo);
        sh->SetFloat("u_Metallic", metallic);
        sh->SetFloat("u_Roughness", roughness);
        sh->SetFloat("u_AO", ao);
        sh->SetVec3("u_Emission", emission);
        sh->SetFloat("u_Alpha", alpha);

        auto albedoTex = d.materialInstance ? d.materialInstance->GetTexture("u_AlbedoTexture") : material->GetTexture("u_AlbedoTexture");
        if (albedoTex && albedoTex->IsValid()) {
            albedoTex->Bind(kAlbedoTextureUnit);
            sh->SetInt("u_AlbedoTexture", kAlbedoTextureUnit);
            sh->SetInt("u_UseAlbedoTexture", 1);
        } else {
            sh->SetInt("u_UseAlbedoTexture", 0);
        }
        auto normalTex = d.materialInstance ? d.materialInstance->GetTexture("u_NormalTexture") : material->GetTexture("u_NormalTexture");
        if (normalTex && normalTex->IsValid()) {
            normalTex->Bind(kNormalTextureUnit);
            sh->SetInt("u_NormalTexture", kNormalTextureUnit);
            sh->SetInt("u_UseNormalTexture", 1);
        } else {
            sh->SetInt("u_UseNormalTexture", 0);
        }
        auto mrTex = d.materialInstance ? d.materialInstance->GetTexture("u_MetallicRoughnessTexture") : material->GetTexture("u_MetallicRoughnessTexture");
        if (mrTex && mrTex->IsValid()) {
            mrTex->Bind(kMetallicRoughnessTextureUnit);
            sh->SetInt("u_MetallicRoughnessTexture", kMetallicRoughnessTextureUnit);
            sh->SetInt("u_UseMetallicRoughnessTexture", 1);
        } else {
            sh->SetInt("u_UseMetallicRoughnessTexture", 0);
        }
        auto aoTex = d.materialInstance ? d.materialInstance->GetTexture("u_AOTexture") : material->GetTexture("u_AOTexture");
        if (aoTex && aoTex->IsValid()) {
            aoTex->Bind(kAOTextureUnit);
            sh->SetInt("u_AOTexture", kAOTextureUnit);
            sh->SetInt("u_UseAOTexture", 1);
        } else {
            sh->SetInt("u_UseAOTexture", 0);
        }
        auto emissiveTex = d.materialInstance ? d.materialInstance->GetTexture("u_EmissiveTexture") : material->GetTexture("u_EmissiveTexture");
        if (emissiveTex && emissiveTex->IsValid()) {
            emissiveTex->Bind(kEmissiveTextureUnit);
            sh->SetInt("u_EmissiveTexture", kEmissiveTextureUnit);
            sh->SetInt("u_UseEmissiveTexture", 1);
        } else {
            sh->SetInt("u_UseEmissiveTexture", 0);
        }

        d.mesh->Bind();
        d.mesh->Draw();
        d.mesh->Unbind();
    });

    m_forwardPlusShader->Unbind();
    if (m_forwardPlusSkinnedShader) m_forwardPlusSkinnedShader->Unbind();
    if (light.castShadows && m_shadowMap && cmdFwd) cmdFwd->UnbindTexture(kShadowMapUnit);
    m_scratchTransparent.clear();
    view.ForEachTransparentDrawable([this](const Drawable& d) { m_scratchTransparent.push_back(d); });
    TransparencyPass::Render(cmdFwd, m_scratchTransparent, cam.view, cam.projection, cam.viewPos);
    if (cmdFwd) { cmdFwd->SetCullFace(true); cmdFwd->SetRenderTarget(nullptr); }
}

void RenderPipeline::Render(const RenderScene& scene) {
    RenderSceneView view(scene);
    Render(view);
}

void RenderPipeline::Render(const IRenderSceneView& view) {
    if (m_width == 0 || m_height == 0) return;
    EnsureResources(m_width, m_height);
    if (!m_hdrRenderTarget || !m_gbuffer) return;

    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;  // No command list available
#ifndef NDEBUG
    m_rendering = true;
#endif
    cmd->Begin();
    // STEP — Prove async GL 1282: flush so any pending error from previous frame is reported here.
    if (m_device) m_device->Finish();

    const CameraView cam = view.GetCamera();
    switch (m_mode) {
    case PipelineMode::Deferred:
        BuildAndExecuteFrameGraph(view);
        break;
    case PipelineMode::Forward:
        BuildAndExecuteForwardFrameGraph(view);
        break;
    case PipelineMode::ForwardPlus:
        RenderForwardPlus(view);
        break;
    }

    if (view.GetSkybox()) {
        const bool useGbufferDepth = (m_mode == PipelineMode::Deferred);
        RenderSkybox(view, useGbufferDepth);
    }

    if (m_prePostCallback) {
        const RenderScene* sceneForCb = view.GetSceneForCallback();
        if (sceneForCb)
            m_prePostCallback(m_hdrRenderTarget, m_width, m_height, *sceneForCb);
    }

    if (eyelib::debug::GetPresets().debugDrawEnabled.load()) {
        rhi::IRenderTarget* target = m_hdrRenderTarget;
        DebugDrawRenderer::Render(cam, target, m_width, m_height);
    }
    
    if (m_postProcessor && m_mode == PipelineMode::ForwardPlus) {
        PostProcessor::Settings settings;
        math::Mat4 invView = glm::inverse(cam.view);
        math::Mat4 invProj = glm::inverse(cam.projection);
        auto skybox = view.GetSkybox();
        rhi::ITexture* skyboxTex = skybox && skybox->IsValid() ? skybox->GetTextureRHI() : nullptr;
        float time = core::Time::TotalTime();
        rhi::ITexture* hdrTex = GetHDRColorTextureRHI();
        rhi::ITexture* finalTexture = m_postProcessor->ApplyPostProcessing(
            hdrTex, *m_gbuffer, settings,
            cam.view, cam.projection,
            invView, invProj, cam.viewPos, skyboxTex, time);
        (void)finalTexture;
    }
    
    m_prevView = cam.view;
    m_prevProjection = cam.projection;

    if (cmd) cmd->End();
#ifndef NDEBUG
    m_rendering = false;
#endif
}

void RenderPipeline::Finish() {
    if (m_device) m_device->Finish();
}

void RenderPipeline::BlitToScreen() {
    if (!m_device) return;
    // Blit scene with cover: fill full window, preserve aspect by cropping (no clear-color bars).
    rhi::ICommandList* cmd = GetRHICommandList();
    if (cmd) {
        rhi::ClearValue bg = {};
        bg.color[0] = 0.12f; bg.color[1] = 0.12f; bg.color[2] = 0.15f; bg.color[3] = 1.f;
        cmd->ClearDefaultFramebuffer(bg);
    }
    rhi::IRenderTarget* blitSource = nullptr;
    if (m_postProcessor && m_postProcessor->GetOutputRenderTargetRHI() && m_postProcessor->HasValidOutputFrame()) {
        blitSource = m_postProcessor->GetOutputRenderTargetRHI();
    } else if (m_hdrRenderTarget) {
        blitSource = m_hdrRenderTarget;
    }
    if (blitSource && m_windowWidth > 0 && m_windowHeight > 0) {
        m_device->BlitToDefaultFramebufferLetterbox(blitSource, m_windowWidth, m_windowHeight);
    } else if (blitSource) {
        m_device->BlitToDefaultFramebuffer(blitSource);
    }
#if 0
    static uint32_t s_blitFrameCount = 0;
    if (++s_blitFrameCount % 60 == 0 && blitSource) {
        core::Log::InfoF("BlitToScreen: source=%s size=%ux%u pipelineSize=%ux%u",
                         (blitSource == m_hdrRenderTarget ? "HDR" : "post"),
                         blitSource->GetWidth(), blitSource->GetHeight(), m_width, m_height);
    }
#endif
    m_device->Present();
}

bool RenderPipeline::HasValidOutput() const {
    return m_hdrRenderTarget != nullptr && m_gbuffer != nullptr;
}

void RenderPipeline::EnsureSkyboxShader() {
    if (m_skyboxShader && m_skyboxShader->IsValid()) return;
    if (!m_device) return;
    m_skyboxShader = std::make_shared<Shader>();
    m_skyboxShader->Load(m_device.get(), "passes/sky/skybox", Mesh::GetFullscreenQuadVertexLayoutRHI());
}

void RenderPipeline::RenderSkyboxImpl(const CameraView& camera, const std::shared_ptr<TextureCube>& skybox, bool useGbufferDepth) {
    if (!skybox || !skybox->IsValid()) return;
    EnsureSkyboxShader();
    if (!m_skyboxShader || !m_skyboxShader->IsValid()) return;

    rhi::ICommandList* cmdSky = GetRHICommandList();
    if (cmdSky && m_hdrRenderTarget) {
        cmdSky->SetRenderTarget(m_hdrRenderTarget);
        cmdSky->SetViewport(0, 0, m_width, m_height);
    }
    if (useGbufferDepth && m_gbuffer && m_device && m_hdrRenderTarget)
        m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_hdrRenderTarget);
    if (cmdSky && m_hdrRenderTarget)
        cmdSky->SetRenderTarget(m_hdrRenderTarget);

    if (cmdSky) { cmdSky->SetDepthTest(true); cmdSky->SetDepthFuncLess(false); cmdSky->SetDepthWrite(false); cmdSky->SetCullFace(false); }

    m_skyboxShader->Bind();
    m_skyboxShader->SetMat4("u_InverseViewMatrix", glm::inverse(camera.view));
    m_skyboxShader->SetMat4("u_InverseProjectionMatrix", glm::inverse(camera.projection));
    m_skyboxShader->SetInt("u_Skybox", 0);
    skybox->Bind(0);

    rhi::IPipeline* pipe = m_skyboxShader->GetPipelineRHI();
    if (pipe && cmdSky)
        cmdSky->DrawFullscreenQuad(pipe);

    m_skyboxShader->Unbind();
    if (cmdSky) { cmdSky->SetDepthWrite(true); cmdSky->SetDepthFuncLess(true); cmdSky->SetCullFace(true); cmdSky->SetRenderTarget(nullptr); }
}

void RenderPipeline::RenderSkybox(const IRenderSceneView& view, bool useGbufferDepth) {
    RenderSkyboxImpl(view.GetCamera(), view.GetSkybox(), useGbufferDepth);
}

GBuffer* RenderPipeline::GetGBuffer() { return m_gbuffer.get(); }
ShadowMap* RenderPipeline::GetShadowMap() { return m_shadowMap.get(); }
PostProcessor* RenderPipeline::GetPostProcessor() { return m_postProcessor.get(); }

const std::vector<FrameGraph::PassInfo>& RenderPipeline::GetFrameGraphPasses() const {
    static const std::vector<FrameGraph::PassInfo> empty;
    return m_frameGraph ? m_frameGraph->GetPasses() : empty;
}

} // namespace render
} // namespace eyelib
