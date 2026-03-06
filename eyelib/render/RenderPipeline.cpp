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
#include "eyelib/render/RenderContext.h"
#include "eyelib/render/2d/Primitives.h"
#include "eyelib/debug/Presets.h"
#include "eyelib/debug/Profiler.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/framegraph/FrameGraphRecipe.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/MaterialInstance.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/TextureCube.h"
#include "eyelib/core/Log.h"
#include "eyelib/core/Time.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
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
    // Pipeline never mutates global state. Caller must run DebugDrawRenderer::Shutdown(), Primitives::ShutdownStaticResources() before Shutdown() when OwnsDevice().
    debug::Profiler::SetGpuProfiler(nullptr);
    m_gpuProfilerRHI.reset();

    DestroyHDRFramebuffer();
    m_postProcessor.reset();
    m_forwardPlusShader.reset();
    m_forwardPlusSkinnedShader.reset();
    m_stereoForwardShader.reset();
    m_outlinePostShader.reset();
    m_entityIdShader.reset();
    if (m_device) {
        if (m_outlineCompositeTemp) { m_device->DestroyRenderTarget(m_outlineCompositeTemp); m_outlineCompositeTemp = nullptr; }
        if (m_entityIdRenderTarget) { m_device->DestroyRenderTarget(m_entityIdRenderTarget); m_entityIdRenderTarget = nullptr; }
    }
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
    if (m_ownedDevice)
        m_ownedDevice.reset();
    m_device = nullptr;
    m_width = m_height = 0;
}

bool RenderPipeline::Initialize(std::unique_ptr<rhi::IDevice> device, uint32_t width, uint32_t height) {
    if (!device) {
        core::Log::Error("RenderPipeline::Initialize: device is null");
        return false;
    }
    m_width = width;
    m_height = height;
    m_ownedDevice = std::move(device);
    m_device = m_ownedDevice.get();
    return InitializeDeviceResources(width, height);
}

bool RenderPipeline::Initialize(rhi::IDevice* sharedDevice, uint32_t width, uint32_t height) {
    if (!sharedDevice) {
        core::Log::Error("RenderPipeline::Initialize(shared): device is null");
        return false;
    }
    m_width = width;
    m_height = height;
    m_device = sharedDevice;
    return InitializeDeviceResources(width, height);
}

bool RenderPipeline::InitializeDeviceResources(uint32_t width, uint32_t height) {
    Primitives::SetStaticDevice(m_device);
    core::Log::Info("RenderPipeline::Initialize: initializing GBuffer");
    m_gbuffer = std::make_unique<GBuffer>();
    if (!m_gbuffer->Initialize(m_device, width, height)) {
        core::Log::Error("RenderPipeline::Initialize: GBuffer::Initialize failed");
        return false;
    }

    core::Log::Info("RenderPipeline::Initialize: initializing ShadowMap");
    m_shadowMap = std::make_unique<ShadowMap>();
    if (!m_shadowMap->Initialize(m_device, 2048)) {
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
    if (m_gpuCullPass->IsSupported(m_device))
        m_gpuCullPass->Initialize(m_device);
    m_lightCullPass = std::make_unique<LightCullPass>();
    if (m_lightCullPass->IsSupported(m_device))
        m_lightCullPass->Initialize(m_device);
    // A2: Transform buffer for GPU cull path; draw list is now inside GpuCullPass combined buffer.
    constexpr uint32_t kMaxGpuCullDraws = 256;
    if (m_device && m_gpuCullPass->IsSupported(m_device)) {
        rhi::BufferDesc bd;
        bd.sizeInBytes = kMaxGpuCullDraws * sizeof(math::Mat4);
        bd.usage = rhi::BufferUsage::Storage;
        bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_gpuCullTransformBuffer = m_device->CreateBuffer(bd, nullptr);
    }
    // Quad for GPU-driven GBuffer path (E8): matches cull_indirect.comp output (6 indices, 1 draw).
    {
        MeshDescriptor quadDesc;
        quadDesc.vertices.resize(4);
        quadDesc.vertices[0].position = math::Vec3(-1.0f, -1.0f, 0.0f);
        quadDesc.vertices[0].uv = math::Vec2(0.0f, 0.0f);
        quadDesc.vertices[1].position = math::Vec3(1.0f, -1.0f, 0.0f);
        quadDesc.vertices[1].uv = math::Vec2(1.0f, 0.0f);
        quadDesc.vertices[2].position = math::Vec3(1.0f, 1.0f, 0.0f);
        quadDesc.vertices[2].uv = math::Vec2(1.0f, 1.0f);
        quadDesc.vertices[3].position = math::Vec3(-1.0f, 1.0f, 0.0f);
        quadDesc.vertices[3].uv = math::Vec2(0.0f, 1.0f);
        quadDesc.indices32 = {0, 1, 2, 2, 3, 0};
        quadDesc.indices16.clear();
        quadDesc.indexFormat = IndexFormat::UInt32;
        quadDesc.submeshes.clear();
        m_gpuDrivenQuad = std::make_unique<Mesh>();
        m_gpuDrivenQuad->CreateFromDescriptor(m_device, quadDesc);
    }
    m_shadowPass = std::make_unique<ShadowPass>();
    m_lightingPass = std::make_unique<LightingPass>();

    // Create pass default shaders once at init (no lazy creation during render).
    {
        auto gbufferShader = std::make_shared<Shader>();
        if (gbufferShader->Load(m_device, "passes/deferred/gbuffer_skinned", Mesh::GetVertexLayoutRHI()))
            m_gbufferPass->SetGeometryShader(std::move(gbufferShader));

        auto shadowShader = std::make_shared<Shader>();
        if (shadowShader->Load(m_device, "passes/shadow/shadow", Mesh::GetVertexLayoutRHI()))
            m_shadowPass->SetDepthShader(std::move(shadowShader));
        auto shadowSkinnedShader = std::make_shared<Shader>();
        if (shadowSkinnedShader->Load(m_device, "passes/shadow/shadow_skinned", Mesh::GetVertexLayoutRHI()))
            m_shadowPass->SetDepthSkinnedShader(std::move(shadowSkinnedShader));

        rhi::VertexLayout fullscreenLayout;
        fullscreenLayout.strideInBytes = 4u * sizeof(float);
        fullscreenLayout.attributes = {
            {0, rhi::VertexAttributeType::Float2, 0},
            {1, rhi::VertexAttributeType::Float2, 2u * sizeof(float)},
        };
        auto lightingShader = std::make_shared<Shader>();
        if (lightingShader->Load(m_device, "passes/deferred/lighting", fullscreenLayout))
            m_lightingPass->SetLightingShader(std::move(lightingShader));
    }

    m_frameGraph = std::make_unique<FrameGraph>();
    m_frameGraph->SetFrameSize(width, height);
    m_clusteredLightGrid = std::make_unique<ClusteredLightGrid>();
    m_clusteredLightGrid->Initialize(width, height, 0.1f, 1000.0f);  // Default, will be updated per-frame

    core::Log::Info("RenderPipeline::Initialize: initializing PostProcessor");
    m_postProcessor = std::make_unique<PostProcessor>();
    if (!m_postProcessor->Initialize(width, height, m_device)) {
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

    m_gpuProfilerRHI = std::make_unique<GpuProfilerRHI>(m_device);
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
    if (m_hdrRenderTarget) {
        rhi::ITexture* depthTex = m_hdrRenderTarget->GetDepthTexture();
        if (depthTex) {
            std::vector<rhi::RHITextureFormat> idColor = { rhi::RHITextureFormat::R32UI };
            m_entityIdRenderTarget = m_device->CreateRenderTargetWithSharedDepth(
                m_width, m_height, idColor, depthTex, 1);
        }
        std::vector<rhi::RHITextureFormat> copyColor = { rhi::RHITextureFormat::RGBA16F };
        m_outlineCompositeTemp = m_device->CreateRenderTarget(
            static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height),
            copyColor, rhi::RHITextureFormat::Depth24, 1);
    }
}

void RenderPipeline::DestroyHDRFramebuffer() {
    // Caller must ensure this is only called at frame boundary (e.g. from Resize), not while a frame is in flight.
    if (m_device) {
        if (m_outlineCompositeTemp) {
            m_device->DestroyRenderTarget(m_outlineCompositeTemp);
            m_outlineCompositeTemp = nullptr;
        }
        if (m_entityIdRenderTarget) {
            m_device->DestroyRenderTarget(m_entityIdRenderTarget);
            m_entityIdRenderTarget = nullptr;
        }
        if (m_hdrRenderTarget) {
            m_device->DestroyRenderTarget(m_hdrRenderTarget);
            m_hdrRenderTarget = nullptr;
        }
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

    if (m_gpuCullPass && m_gpuCullPass->IsSupported(m_device)) {
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
    RenderContext ctx = RenderContext::FromDevice(m_device);
    if (!ctx) return;
    const bool useIndirectQuad = m_useGpuDrivenGBuffer && m_gpuCullPass && m_gpuCullPass->IsSupported(m_device)
        && m_gpuDrivenQuad && m_gpuDrivenQuad->IsValid() && (gpuCullDrawListCount == 0);
    const bool useIndirectScene = m_useGpuDrivenGBuffer && m_gpuCullPass && m_gpuCullPass->IsSupported(m_device)
        && gpuCullDrawListCount > 0 && m_gpuCullSingleMesh && m_gpuCullRepresentativeMesh && m_gpuCullRepresentativeMesh->IsValid()
        && m_gpuCullTransformBuffer;
    if (useIndirectQuad) {
        m_gbufferPass->RenderIndirect(*m_gbuffer, ctx,
            m_gpuCullPass->GetIndirectBuffer(),
            m_gpuCullPass->GetDrawCount(),
            m_gpuCullPass->GetIndirectStride(),
            m_gpuDrivenQuad.get(),
            cam.view, cam.projection, &m_prevView, &m_prevProjection);
    } else if (useIndirectScene) {
        m_gbufferPass->RenderIndirectWithPerDrawData(*m_gbuffer, ctx,
            m_gpuCullPass->GetIndirectBuffer(),
            m_gpuCullPass->GetDrawCount(),
            m_gpuCullPass->GetIndirectStride(),
            m_gpuCullRepresentativeMesh.get(),
            m_gpuCullTransformBuffer,
            gpuCullDrawListCount,
            cam.view, cam.projection, &m_prevView, &m_prevProjection);
    } else {
        m_gbufferPass->Render(*m_gbuffer, ctx,
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

    m_scratchPointLights.clear();
    m_scratchSpotLights.clear();
    view.ForEachPointLight([this](const PointLight& p) { m_scratchPointLights.push_back(p); });
    view.ForEachSpotLight([this](const SpotLight& s) { m_scratchSpotLights.push_back(s); });

    FGNameToHandle nameToHandle;
    rhi::ITexture* hdrColorTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
    rhi::ITexture* hdrDepthTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetDepthTexture() : nullptr;
    nameToHandle["HDR"] = m_frameGraph->ImportTextureRHI("HDR", hdrColorTex, m_width, m_height);
    nameToHandle["HDRDepth"] = m_frameGraph->ImportTextureRHI("HDRDepth", hdrDepthTex, m_width, m_height);
    nameToHandle["GBufferPos"] = m_frameGraph->ImportTextureRHI("GBufferPos", m_gbuffer->GetPositionTextureRHI(), m_width, m_height);
    nameToHandle["GBufferNorm"] = m_frameGraph->ImportTextureRHI("GBufferNorm", m_gbuffer->GetNormalTextureRHI(), m_width, m_height);
    nameToHandle["GBufferAlb"] = m_frameGraph->ImportTextureRHI("GBufferAlb", m_gbuffer->GetAlbedoTextureRHI(), m_width, m_height);
    nameToHandle["GBufferPBR"] = m_frameGraph->ImportTextureRHI("GBufferPBR", m_gbuffer->GetPBRParamsTextureRHI(), m_width, m_height);
    nameToHandle["GBufferVelocity"] = m_frameGraph->ImportTextureRHI("GBufferVelocity", m_gbuffer->GetVelocityTextureRHI(), m_width, m_height);
    nameToHandle["GBufferDepth"] = m_frameGraph->ImportTextureRHI("GBufferDepth", m_gbuffer->GetDepthTextureRHI(), m_width, m_height);
    nameToHandle["Shadow"] = m_frameGraph->ImportTextureRHI("Shadow", m_shadowMap->GetDepthTextureRHI(), m_shadowMap->GetResolution(), m_shadowMap->GetResolution());

    RenderContext ctx = RenderContext::FromDevice(m_device);
    const uint32_t gpuCullDrawListCount = PrepareGpuCullFromView(view, cam);
    const rhi::ClearValue clearBlack = { 0.f, 0.f, 0.f, 1.f };

    FrameGraphRecipe recipe;
    recipe.push_back({ "GPU Cull", {}, {}, true });
    if (light.castShadows)
        recipe.push_back({ "Shadow", {}, { "Shadow" }, true });
    recipe.push_back({ "GBuffer", {}, { "GBufferPos", "GBufferNorm", "GBufferAlb", "GBufferPBR", "GBufferVelocity", "GBufferDepth" }, true });
    recipe.push_back({ "Copy depth for occlusion", { "GBufferDepth" }, {}, true });
    std::vector<std::string> lightingReads = { "GBufferPos", "GBufferNorm", "GBufferAlb", "GBufferPBR" };
    if (light.castShadows) lightingReads.push_back("Shadow");
    recipe.push_back({ "Lighting", lightingReads, { "HDR" }, true });
    recipe.push_back({ "Transparency", {}, { "HDR" }, true });

    auto getExecutor = [this, &view, &cam, &light, ctx, gpuCullDrawListCount, clearBlack](const std::string& name) -> FrameGraph::ExecuteFunc {
        if (name == "GPU Cull")
            return [this, ctx](const FGPassResources&) { if (m_gpuCullPass) m_gpuCullPass->Execute(ctx); };
        if (name == "Shadow")
            return [this, &view, &light, &cam, ctx](const FGPassResources&) {
                m_shadowPass->Render(*m_shadowMap, ctx,
                    [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
                    light.direction, cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
            };
        if (name == "GBuffer")
            return [this, &view, &cam, ctx, gpuCullDrawListCount](const FGPassResources&) {
                RenderOpaqueGeometryToGBuffer(ctx.cmd, view, cam, gpuCullDrawListCount);
            };
        if (name == "Copy depth for occlusion")
            return [this](const FGPassResources&) {
                if (m_prevFrameDepthRT && m_gbuffer && m_gbuffer->GetRenderTargetRHI())
                    m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_prevFrameDepthRT);
                m_havePrevDepth = (m_prevFrameDepthRT != nullptr);
            };
        if (name == "Lighting")
            return [this, &view, &cam, ctx, clearBlack](const FGPassResources&) {
                if (ctx.cmd && m_hdrRenderTarget)
                    rhi::ClearAndSetRenderTargetWithViewport(ctx.cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearBlack);
                ShadowMap* shadowMapForLighting = view.GetLight().castShadows ? m_shadowMap.get() : nullptr;
                m_lightingPass->Render(*m_gbuffer, ctx, view.GetLight(), cam.view, cam.projection, cam.viewPos, shadowMapForLighting,
                    &m_scratchPointLights, &m_scratchSpotLights);
            };
        if (name == "Transparency")
            return [this, &view, &cam, ctx](const FGPassResources&) {
                if (m_device && m_hdrRenderTarget)
                    m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_hdrRenderTarget);
                if (ctx.cmd && m_hdrRenderTarget) {
                    ctx.cmd->SetRenderTarget(m_hdrRenderTarget);
                    ctx.cmd->SetViewport(0, 0, m_width, m_height);
                }
                m_scratchTransparent.clear();
                view.ForEachTransparentDrawable([this](const Drawable& d) { m_scratchTransparent.push_back(d); });
                TransparencyPass::Render(ctx, m_scratchTransparent, cam.view, cam.projection, cam.viewPos);
            };
        return [](const FGPassResources&) {};
    };

    AddPassesFromRecipe(*m_frameGraph, recipe, nameToHandle, getExecutor);
    AddSkyboxPassToFrameGraph(view, nameToHandle["HDR"], true);

    m_frameGraph->Compile(m_device);
    m_frameGraph->Execute(ctx);
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

    FGNameToHandle nameToHandle;
    rhi::ITexture* hdrColorTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
    nameToHandle["HDR"] = m_frameGraph->ImportTextureRHI("HDR", hdrColorTex, m_width, m_height);
    nameToHandle["Shadow"] = m_frameGraph->ImportTextureRHI("Shadow", m_shadowMap->GetDepthTextureRHI(), m_shadowMap->GetResolution(), m_shadowMap->GetResolution());

    RenderContext ctx = RenderContext::FromDevice(m_device);
    const uint32_t gpuCullDrawListCount = PrepareGpuCullFromView(view, cam);
    const rhi::ClearValue clearFwd = { 0.1f, 0.1f, 0.15f, 1.f };

    FrameGraphRecipe recipe;
    if (m_gpuCullPass && m_gpuCullPass->IsSupported(m_device))
        recipe.push_back({ "GPU Cull", {}, {}, true });
    if (light.castShadows)
        recipe.push_back({ "Shadow", {}, { "Shadow" }, true });
    recipe.push_back({ "Forward Opaque", {}, { "HDR" }, true });
    recipe.push_back({ "Copy depth for occlusion", { "HDR" }, {}, true });
    recipe.push_back({ "Transparency", {}, { "HDR" }, true });

    auto getExecutor = [this, &view, &cam, &light, ctx, clearFwd](const std::string& name) -> FrameGraph::ExecuteFunc {
        if (name == "GPU Cull")
            return [this, ctx](const FGPassResources&) { if (m_gpuCullPass) m_gpuCullPass->Execute(ctx); };
        if (name == "Shadow")
            return [this, &view, &light, &cam, ctx](const FGPassResources&) {
                m_shadowPass->Render(*m_shadowMap, ctx,
                    [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
                    light.direction, cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
            };
        if (name == "Forward Opaque")
            return [this, &view, ctx, clearFwd](const FGPassResources&) {
                if (ctx.cmd && m_hdrRenderTarget)
                    rhi::ClearAndSetRenderTargetWithViewport(ctx.cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearFwd);
                if (ctx.cmd) { ctx.cmd->SetDepthTest(true); ctx.cmd->SetCullFace(false); }
                RenderForwardOpaque(ctx.cmd, view);
            };
        if (name == "Copy depth for occlusion")
            return [this](const FGPassResources&) {
                if (m_prevFrameDepthRT && m_hdrRenderTarget)
                    m_device->BlitDepthOnly(m_hdrRenderTarget, m_prevFrameDepthRT);
                m_havePrevDepth = (m_prevFrameDepthRT != nullptr);
            };
        if (name == "Transparency")
            return [this, &view, &cam, ctx](const FGPassResources&) {
                if (ctx.cmd && m_hdrRenderTarget) {
                    ctx.cmd->SetRenderTarget(m_hdrRenderTarget);
                    ctx.cmd->SetViewport(0, 0, m_width, m_height);
                }
                m_scratchTransparent.clear();
                view.ForEachTransparentDrawable([this](const Drawable& d) { m_scratchTransparent.push_back(d); });
                TransparencyPass::Render(ctx, m_scratchTransparent, cam.view, cam.projection, cam.viewPos);
            };
        return [](const FGPassResources&) {};
    };

    AddPassesFromRecipe(*m_frameGraph, recipe, nameToHandle, getExecutor);
    AddSkyboxPassToFrameGraph(view, nameToHandle["HDR"], false);

    m_frameGraph->Compile(m_device);
    m_frameGraph->Execute(ctx);
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
            d.materialInstance->Bind(m_device);
        } else {
            d.material->Bind(m_device);
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
        d.mesh->Bind(m_device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device); }
        d.mesh->Unbind();
        sh->Unbind();
    });

    if (useShadows && cmd) cmd->UnbindTexture(kShadowMapUnit);
}

void RenderPipeline::RenderForwardOpaqueStereo(rhi::ICommandList* cmd, const IRenderSceneView& view, const CameraView& leftCam, const CameraView& rightCam) {
    EnsureStereoForwardShader();
    if (!m_stereoForwardShader || !m_stereoForwardShader->IsValid()) return;

    const LightParams light = view.GetLight();
    const int kShadowMapUnit = static_cast<int>(rhi::PassBindings::ShadowMap);
    const bool useShadows = (m_shadowMap != nullptr && light.castShadows);
    if (useShadows && cmd) {
        rhi::ITexture* shadowTex = m_shadowMap->GetDepthTextureRHI();
        if (shadowTex) cmd->BindTexture(shadowTex, kShadowMapUnit);
    }

    const uint32_t halfW = m_width / 2;
    if (cmd) {
        cmd->SetViewportIndexed(0, 0, 0, halfW, m_height);
        cmd->SetViewportIndexed(1, halfW, 0, halfW, m_height);
    }

    const int kMaxPointLights = 8;
    const int kMaxSpotLights = 4;
    const int numPoint = static_cast<int>(std::min(m_scratchPointLights.size(), size_t(kMaxPointLights)));
    const int numSpot = static_cast<int>(std::min(m_scratchSpotLights.size(), size_t(kMaxSpotLights)));

    math::Mat4 viewArr[2] = { leftCam.view, rightCam.view };
    math::Mat4 projArr[2] = { leftCam.projection, rightCam.projection };

    view.ForEachOpaqueDrawable([&](const Drawable& d) {
        if (!d.mesh || !d.material) return;
        Material* mat = (d.materialInstance ? d.materialInstance->GetMaterial().get() : d.material.get());
        if (!mat) return;

        m_stereoForwardShader->Bind(m_device);
        Shader* sh = m_stereoForwardShader.get();
        sh->SetMat4("u_Model", d.transform);
        sh->SetMat4Array("u_View", viewArr, 2);
        sh->SetMat4Array("u_Projection", projArr, 2);
        sh->SetVec3("u_ViewPos[0]", leftCam.viewPos);
        sh->SetVec3("u_ViewPos[1]", rightCam.viewPos);
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
        sh->SetVec3("u_Albedo", mat->GetAlbedo());
        sh->SetFloat("u_Metallic", mat->GetMetallic());
        sh->SetFloat("u_Roughness", mat->GetRoughness());
        sh->SetVec3("u_Emission", mat->GetEmission());
        sh->SetFloat("u_Alpha", mat->GetAlpha());
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
        d.mesh->Bind(m_device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.DrawInstanced(m_device, -1, 2); }
        d.mesh->Unbind();
        m_stereoForwardShader->Unbind();
    });

    if (useShadows && cmd) cmd->UnbindTexture(kShadowMapUnit);
}

void RenderPipeline::BuildAndExecuteForwardFrameGraphStereo(const IRenderSceneView& view, const CameraView& leftCam, const CameraView& rightCam) {
    if (!m_frameGraph || !m_shadowMap || !m_shadowPass || !m_hdrRenderTarget) return;
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);

    const LightParams light = view.GetLight();
    m_scratchPointLights.clear();
    m_scratchSpotLights.clear();
    view.ForEachPointLight([this](const PointLight& p) { m_scratchPointLights.push_back(p); });
    view.ForEachSpotLight([this](const SpotLight& s) { m_scratchSpotLights.push_back(s); });

    FGNameToHandle nameToHandle;
    rhi::ITexture* hdrColorTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
    nameToHandle["HDR"] = m_frameGraph->ImportTextureRHI("HDR", hdrColorTex, m_width, m_height);
    nameToHandle["Shadow"] = m_frameGraph->ImportTextureRHI("Shadow", m_shadowMap->GetDepthTextureRHI(), m_shadowMap->GetResolution(), m_shadowMap->GetResolution());

    RenderContext ctx = RenderContext::FromDevice(m_device);
    const uint32_t gpuCullDrawListCount = PrepareGpuCullFromView(view, leftCam);
    const rhi::ClearValue clearFwd = { 0.1f, 0.1f, 0.15f, 1.f };

    FrameGraphRecipe recipe;
    if (light.castShadows)
        recipe.push_back({ "Shadow", {}, { "Shadow" }, true });
    recipe.push_back({ "Forward Opaque", {}, { "HDR" }, true });
    recipe.push_back({ "Transparency", {}, { "HDR" }, true });

    auto getExecutor = [this, &view, &leftCam, &rightCam, &light, ctx, clearFwd](const std::string& name) -> FrameGraph::ExecuteFunc {
        if (name == "Shadow")
            return [this, &view, &light, &leftCam, ctx](const FGPassResources&) {
                m_shadowPass->Render(*m_shadowMap, ctx,
                    [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
                    light.direction, leftCam.view, leftCam.projection, leftCam.viewPos, leftCam.nearPlane, leftCam.farPlane);
            };
        if (name == "Forward Opaque")
            return [this, &view, &leftCam, &rightCam, ctx, clearFwd](const FGPassResources&) {
                if (ctx.cmd && m_hdrRenderTarget)
                    rhi::ClearAndSetRenderTargetWithViewport(ctx.cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearFwd);
                if (ctx.cmd) { ctx.cmd->SetDepthTest(true); ctx.cmd->SetCullFace(false); }
                RenderForwardOpaqueStereo(ctx.cmd, view, leftCam, rightCam);
            };
        if (name == "Transparency")
            return [this, &view, &leftCam, &rightCam, ctx](const FGPassResources&) {
                if (ctx.cmd && m_hdrRenderTarget) {
                    ctx.cmd->SetRenderTarget(m_hdrRenderTarget);
                    const uint32_t halfW = m_width / 2;
                    ctx.cmd->SetViewport(0, 0, halfW, m_height);
                }
                m_scratchTransparent.clear();
                view.ForEachTransparentDrawable([this](const Drawable& d) { m_scratchTransparent.push_back(d); });
                TransparencyPass::Render(ctx, m_scratchTransparent, leftCam.view, leftCam.projection, leftCam.viewPos);
                if (ctx.cmd && m_hdrRenderTarget) {
                    const uint32_t halfW = m_width / 2;
                    ctx.cmd->SetViewport(halfW, 0, halfW, m_height);
                }
                TransparencyPass::Render(ctx, m_scratchTransparent, rightCam.view, rightCam.projection, rightCam.viewPos);
            };
        return [](const FGPassResources&) {};
    };

    AddPassesFromRecipe(*m_frameGraph, recipe, nameToHandle, getExecutor);
    if (view.GetSkybox()) {
        m_frameGraph->AddPass("SkyboxLeft", [this, &view, &leftCam](const FGPassResources&) {
            rhi::ICommandList* cmd = GetRHICommandList();
            if (cmd && m_hdrRenderTarget) { cmd->SetRenderTarget(m_hdrRenderTarget); cmd->SetViewport(0, 0, m_width / 2, m_height); }
            RenderSkyboxImpl(leftCam, view.GetSkybox(), false);
        }, {}, { nameToHandle["HDR"] }, true);
        m_frameGraph->AddPass("SkyboxRight", [this, &view, &rightCam](const FGPassResources&) {
            rhi::ICommandList* cmd = GetRHICommandList();
            if (cmd && m_hdrRenderTarget) { cmd->SetRenderTarget(m_hdrRenderTarget); cmd->SetViewport(m_width / 2, 0, m_width / 2, m_height); }
            RenderSkyboxImpl(rightCam, view.GetSkybox(), false);
        }, {}, { nameToHandle["HDR"] }, true);
    }

    m_frameGraph->Compile(m_device);
    m_frameGraph->Execute(ctx);
}

void RenderPipeline::RenderStereo(const IRenderSceneView& view, const CameraView& leftCam, const CameraView& rightCam) {
    if (m_width == 0 || m_height == 0) return;
    EnsureResources(m_width, m_height);
    if (!m_hdrRenderTarget) return;

    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;
#ifndef NDEBUG
    m_rendering = true;
#endif
    cmd->Begin();
    if (m_device) m_device->Finish();

    BuildAndExecuteForwardFrameGraphStereo(view, leftCam, rightCam);

    const CameraView cam = leftCam;
    if (m_prePostCallback) {
        const RenderScene* sceneForCb = view.GetSceneForCallback();
        if (sceneForCb)
            m_prePostCallback(m_hdrRenderTarget, m_width, m_height, *sceneForCb);
    }

    if (eyelib::debug::GetPresets().debugDrawEnabled.load() && m_device) {
        RenderContext ctx = RenderContext::FromDevice(m_device);
        cmd->SetViewport(0, 0, m_width, m_height);
        DebugDrawRenderer::Render(ctx, leftCam, m_hdrRenderTarget, m_width, m_height);
    }

    if (m_postProcessor) {
        PostProcessor::Settings settings;
        math::Mat4 invView = glm::inverse(cam.view);
        math::Mat4 invProj = glm::inverse(cam.projection);
        auto skybox = view.GetSkybox();
        rhi::ITexture* skyboxTex = skybox && skybox->IsValid() ? skybox->GetTextureRHI() : nullptr;
        float time = core::Time::TotalTime();
        rhi::ITexture* hdrTex = GetHDRColorTextureRHI();
        m_postProcessor->ApplyPostProcessing(hdrTex, *m_gbuffer, settings, cam.view, cam.projection, invView, invProj, cam.viewPos, skyboxTex, time);
    }

    m_prevView = cam.view;
    m_prevProjection = cam.projection;
    cmd->End();
#ifndef NDEBUG
    m_rendering = false;
#endif
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
    RenderContext ctx = RenderContext::FromDevice(m_device);
    TransparencyPass::Render(ctx, m_scratchTransparent, cam.view, cam.projection, cam.viewPos);
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
    if (m_lightCullPass && m_lightCullPass->IsSupported(m_device) && m_lightIndexBuffer) {
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
        RenderContext lightCullCtx = RenderContext::FromDevice(m_device);
        m_lightCullPass->Execute(lightCullCtx);
    } else {
        m_clusteredLightGrid->BuildLightGrid(m_scratchPointLights, m_scratchSpotLights,
                                             cam.view, cam.projection, cam.viewPos);
        m_clusteredLightGrid->UploadToGPU(m_lightIndexBuffer);
    }
    if (cmdFwd && m_lightIndexBuffer) cmdFwd->BindStorageBuffer(m_lightIndexBuffer, 2);

    if (light.castShadows && m_shadowMap && m_shadowPass) {
        RenderContext ctx = RenderContext::FromDevice(m_device);
        m_shadowPass->Render(*m_shadowMap, ctx,
            [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
            light.direction, cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
    }

    // Ensure forward+ shader exists (backend-owned loading by logical name)
    if (!m_forwardPlusShader || !m_forwardPlusShader->IsValid()) {
        m_forwardPlusShader = std::make_shared<Shader>();
        if (m_device)
            m_forwardPlusShader->Load(m_device, "passes/forward/forwardplus", Mesh::GetVertexLayoutRHI());
    }

    if (!m_forwardPlusShader->IsValid()) {
        RenderForward(view);
        return;
    }

    if (!m_forwardPlusSkinnedShader || !m_forwardPlusSkinnedShader->IsValid()) {
        m_forwardPlusSkinnedShader = std::make_shared<Shader>();
        if (m_device)
            m_forwardPlusSkinnedShader->Load(m_device, "passes/forward/forwardplus_skinned", Mesh::GetVertexLayoutRHI());
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
        sh->Bind(m_device);

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

        d.mesh->Bind(m_device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device); }
        d.mesh->Unbind();
    });

    m_forwardPlusShader->Unbind();
    if (m_forwardPlusSkinnedShader) m_forwardPlusSkinnedShader->Unbind();
    if (light.castShadows && m_shadowMap && cmdFwd) cmdFwd->UnbindTexture(kShadowMapUnit);
    m_scratchTransparent.clear();
    view.ForEachTransparentDrawable([this](const Drawable& d) { m_scratchTransparent.push_back(d); });
    RenderContext ctxFwd = RenderContext::FromDevice(m_device);
    TransparencyPass::Render(ctxFwd, m_scratchTransparent, cam.view, cam.projection, cam.viewPos);
    if (cmdFwd) { cmdFwd->SetCullFace(true); cmdFwd->SetRenderTarget(nullptr); }
}

void RenderPipeline::Render(const RenderScene& scene) {
    RenderSceneView view(scene);
    Render(view);
}

void RenderPipeline::Render(const IRenderSceneView& view, const std::vector<uint32_t>* entityIdsForOpaque) {
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

    // EntityID pass: same depth as opaque, write R32UI for picking (editor). Run only when entity IDs provided and count matches.
    if (entityIdsForOpaque && m_entityIdRenderTarget) {
        size_t opaqueCount = 0;
        view.ForEachOpaqueDrawable([&opaqueCount](const Drawable&) { ++opaqueCount; });
        if (opaqueCount == entityIdsForOpaque->size())
            RenderEntityIDPass(view, *entityIdsForOpaque);
    }

    // Draw skybox to HDR with depth LEQUAL (behind geometry). When post is used, the frame-graph Skybox pass already filled HDR; we only draw after the graph when output is HDR (no post).
    if (view.GetSkybox() && GetCurrentFrameOutputRT() == m_hdrRenderTarget) {
        const bool useGbufferDepth = (m_mode == PipelineMode::Deferred);
        RenderSkybox(view, useGbufferDepth, m_hdrRenderTarget);
    }

    if (m_prePostCallback) {
        const RenderScene* sceneForCb = view.GetSceneForCallback();
        if (sceneForCb)
            m_prePostCallback(m_hdrRenderTarget, m_width, m_height, *sceneForCb);
    }

    if (eyelib::debug::GetPresets().debugDrawEnabled.load() && m_device) {
        RenderContext ctx = RenderContext::FromDevice(m_device);
        rhi::IRenderTarget* target = m_hdrRenderTarget;
        DebugDrawRenderer::Render(ctx, cam, target, m_width, m_height);
    }

    if (!m_pendingDebugLinesColored.empty() && m_device) {
        RenderContext ctx = RenderContext::FromDevice(m_device);
        CameraView pendingCam;
        pendingCam.view = m_pendingDebugView;
        pendingCam.projection = m_pendingDebugProj;
        pendingCam.viewPos = math::Vec3(0, 0, 0);
        DebugDrawRenderer::RenderLinesWithDepth(ctx, pendingCam, m_hdrRenderTarget, m_width, m_height, m_pendingDebugLinesColored);
        m_pendingDebugLinesColored.clear();
    }

    // Post runs exactly once, after all scene and debug passes. One path for all modes.
    if (m_postProcessor) {
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

rhi::IRenderTarget* RenderPipeline::GetCurrentFrameOutputRT() const {
    if (m_postProcessor && m_postProcessor->GetOutputRenderTargetRHI() && m_postProcessor->HasValidOutputFrame())
        return m_postProcessor->GetOutputRenderTargetRHI();
    return m_hdrRenderTarget;
}

bool RenderPipeline::ReadOutputPixels(uint32_t width, uint32_t height, uint8_t* outRGBA) const {
    rhi::IRenderTarget* rt = GetCurrentFrameOutputRT();
    if (!m_device || !rt || !outRGBA || width == 0 || height == 0) return false;
    const uint32_t rtW = rt->GetWidth();
    const uint32_t rtH = rt->GetHeight();
    if (rtW == 0 || rtH == 0) return false;
    const uint32_t w = (width <= rtW) ? width : rtW;
    const uint32_t h = (height <= rtH) ? height : rtH;
    return m_device->ReadPixelsRGBA8(rt, 0, 0, w, h, outRGBA);
}

rhi::ITexture* RenderPipeline::GetCurrentFrameOutputColorTextureRHI() const {
    rhi::IRenderTarget* rt = GetCurrentFrameOutputRT();
    return rt ? rt->GetColorTexture(0) : nullptr;
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
    rhi::IRenderTarget* blitSource = GetCurrentFrameOutputRT();
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

void RenderPipeline::EnsureStereoForwardShader() {
    if (m_stereoForwardShader && m_stereoForwardShader->IsValid()) return;
    if (!m_device) return;
    m_stereoForwardShader = std::make_shared<Shader>();
    if (!m_stereoForwardShader->Load(m_device, "passes/forward/forward_stereo", Mesh::GetVertexLayoutRHI())) {
        core::Log::WarnF("Stereo forward shader failed to load (passes/forward/forward_stereo). Stereo preview will fall back to dual pass.");
        m_stereoForwardShader.reset();
    }
}

void RenderPipeline::EnsureSkyboxShader() {
    if (m_skyboxShader && m_skyboxShader->IsValid()) return;
    if (!m_device) return;
    m_skyboxShader = std::make_shared<Shader>();
    rhi::DepthStencilState skyboxDepth;
    skyboxDepth.depthTest = true;
    skyboxDepth.depthWrite = false;
    skyboxDepth.depthCompare = rhi::DepthCompare::LessEqual;
    if (!m_skyboxShader->Load(m_device, "passes/sky/skybox", Mesh::GetFullscreenQuadVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList, &skyboxDepth)) {
        core::Log::WarnF("Skybox shader failed to load (passes/sky/skybox). Run from project root or ensure assets/shaders/opengl/ is available.");
        return;
    }
    if (!m_skyboxShader->IsValid()) {
        core::Log::Warn("Skybox shader invalid after load.");
        return;
    }
}

void RenderPipeline::AddSkyboxPassToFrameGraph(const IRenderSceneView& view, FGTextureHandle hdrColor, bool useGbufferDepth) {
    if (!view.GetSkybox()) return;
    m_frameGraph->AddPass("Skybox",
        [this, &view, useGbufferDepth](const FGPassResources&) {
            RenderSkybox(view, useGbufferDepth);
        },
        {}, { hdrColor }, true);
}

void RenderPipeline::RenderSkyboxImpl(const CameraView& camera, const std::shared_ptr<TextureCube>& skybox, bool useGbufferDepth, rhi::IRenderTarget* targetOverride) {
    if (!skybox || !skybox->IsValid()) {
        static bool logged = false;
        if (!logged) { core::Log::WarnF("RenderSkybox: skipped (no skybox or texture invalid)"); logged = true; }
        return;
    }
    EnsureSkyboxShader();
    if (!m_skyboxShader || !m_skyboxShader->IsValid()) {
        static bool logged = false;
        if (!logged) { core::Log::WarnF("RenderSkybox: skipped (skybox shader not loaded)"); logged = true; }
        return;
    }

    rhi::IRenderTarget* rt = targetOverride ? targetOverride : m_hdrRenderTarget;
    rhi::ICommandList* cmdSky = GetRHICommandList();
    if (cmdSky && rt) {
        cmdSky->SetRenderTarget(rt);
        cmdSky->SetViewport(0, 0, m_width, m_height);
    }
    if (rt == m_hdrRenderTarget && useGbufferDepth && m_gbuffer && m_device && m_hdrRenderTarget)
        m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_hdrRenderTarget);
    if (cmdSky && rt)
        cmdSky->SetRenderTarget(rt);

    if (cmdSky)
        cmdSky->SetCullFace(false);

    m_skyboxShader->Bind(m_device);
    m_skyboxShader->SetMat4("u_InverseViewMatrix", glm::inverse(camera.view));
    m_skyboxShader->SetMat4("u_InverseProjectionMatrix", glm::inverse(camera.projection));
    m_skyboxShader->SetInt("u_Skybox", 0);
    skybox->Bind(0);

    rhi::IPipeline* pipe = m_skyboxShader->GetPipelineRHI();
    if (pipe && cmdSky)
        cmdSky->DrawFullscreenQuad(pipe);

    m_skyboxShader->Unbind();
    if (cmdSky) {
        cmdSky->SetDepthTest(true);
        cmdSky->SetDepthWrite(true);
        cmdSky->SetDepthFuncLess(true);
        cmdSky->SetCullFace(true);
        cmdSky->SetRenderTarget(nullptr);
    }
}

void RenderPipeline::RenderSkybox(const IRenderSceneView& view, bool useGbufferDepth, rhi::IRenderTarget* targetOverride) {
    RenderSkyboxImpl(view.GetCamera(), view.GetSkybox(), useGbufferDepth, targetOverride);
}

void RenderPipeline::EnsureOutlinePostShader() {
    if (m_outlinePostShader && m_outlinePostShader->IsValid()) return;
    if (!m_device) return;
    m_outlinePostShader = std::make_shared<Shader>();
    rhi::DepthStencilState postDepth;
    postDepth.depthTest = false;
    postDepth.depthWrite = false;
    if (!m_outlinePostShader->Load(m_device, "passes/outline_post/outline_post", Mesh::GetFullscreenQuadVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList, &postDepth)) {
        core::Log::WarnF("Outline post shader failed to load (passes/outline_post/outline_post). Run from project root or ensure assets/shaders/opengl/ is available.");
        return;
    }
}

void RenderPipeline::EnsureEntityIdShader() {
    if (m_entityIdShader && m_entityIdShader->IsValid()) return;
    if (!m_device) return;
    m_entityIdShader = std::make_shared<Shader>();
    if (!m_entityIdShader->Load(m_device, "passes/entityid/entityid", Mesh::GetVertexLayoutRHI())) {
        core::Log::WarnF("EntityID shader failed to load (passes/entityid/entityid). Run from project root or ensure assets/shaders/opengl/ is available.");
        return;
    }
}

void RenderPipeline::RenderEntityIDPass(const IRenderSceneView& view, const std::vector<uint32_t>& entityIdsForOpaque) {
    if (!m_entityIdRenderTarget || entityIdsForOpaque.empty()) return;
    EnsureEntityIdShader();
    if (!m_entityIdShader || !m_entityIdShader->IsValid()) return;

    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;

    const CameraView cam = view.GetCamera();
    cmd->SetRenderTarget(m_entityIdRenderTarget);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(true);

    rhi::ClearValue clearId = {};
    clearId.colorUint = 0u;
    cmd->ClearRenderTarget(m_entityIdRenderTarget, clearId);

    m_entityIdShader->Bind(m_device);
    m_entityIdShader->SetMat4("u_View", cam.view);
    m_entityIdShader->SetMat4("u_Projection", cam.projection);

    size_t index = 0;
    view.ForEachOpaqueDrawable([&](const Drawable& d) {
        if (!d.mesh || index >= entityIdsForOpaque.size()) { ++index; return; }
        uint32_t id = entityIdsForOpaque[index++];
        m_entityIdShader->SetUint("u_EntityID", id);
        m_entityIdShader->SetMat4("u_Model", d.transform);
        d.mesh->Bind(m_device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device); }
        d.mesh->Unbind();
    });

    m_entityIdShader->Unbind();
    cmd->SetDepthWrite(true);
    cmd->SetRenderTarget(nullptr);
}

void RenderPipeline::RenderOutlineFromIdBuffer(uint32_t selectedPackedId, float r, float g, float b) {
    if (selectedPackedId == 0 || !m_hdrRenderTarget || !m_entityIdRenderTarget || !m_outlineCompositeTemp || !m_device) return;
    EnsureOutlinePostShader();
    if (!m_outlinePostShader || !m_outlinePostShader->IsValid()) return;

    m_device->Blit(m_hdrRenderTarget, m_outlineCompositeTemp, true);

    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;

    cmd->SetRenderTarget(m_hdrRenderTarget);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);

    m_outlinePostShader->Bind(m_device);
    rhi::ITexture* sceneTex = m_outlineCompositeTemp->GetColorTexture(0);
    rhi::ITexture* idTex = m_entityIdRenderTarget->GetColorTexture(0);
    if (sceneTex) cmd->BindTexture(sceneTex, 0);
    if (idTex) cmd->BindTexture(idTex, 1);
    m_outlinePostShader->SetUint("u_SelectedID", selectedPackedId);
    m_outlinePostShader->SetVec3("u_OutlineColor", math::Vec3(r, g, b));
    const float invW = m_width > 0 ? 1.0f / static_cast<float>(m_width) : 0.0f;
    const float invH = m_height > 0 ? 1.0f / static_cast<float>(m_height) : 0.0f;
    m_outlinePostShader->SetVec2("u_InvSize", math::Vec2(invW, invH));

    rhi::IPipeline* pipe = m_outlinePostShader->GetPipelineRHI();
    if (pipe) cmd->DrawFullscreenQuad(pipe);

    m_outlinePostShader->Unbind();
    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(true);
    cmd->SetCullFace(true);
    cmd->SetRenderTarget(nullptr);
}

void RenderPipeline::RenderDebugLines(const math::Mat4& view, const math::Mat4& proj,
    const std::vector<std::pair<math::Vec3, math::Vec3>>& lines, float r, float g, float b) {
    // Always draw into HDR so depth test runs against scene depth; post then consumes HDR (with lines).
    rhi::IRenderTarget* rt = m_hdrRenderTarget;
    if (lines.empty() || !rt || m_width == 0 || m_height == 0) return;
    RenderContext ctx = RenderContext::FromDevice(m_device);
    CameraView cam;
    cam.view = view;
    cam.projection = proj;
    cam.viewPos = math::Vec3(0, 0, 0);
    DebugDrawRenderer::RenderLinesWithDepth(ctx, cam, rt, m_width, m_height, lines, math::Vec4(r, g, b, 1.f));
}

void RenderPipeline::SetPendingDebugLines(const math::Mat4& view, const math::Mat4& proj,
    const std::vector<std::pair<math::Vec3, math::Vec3>>& lines, float r, float g, float b) {
    m_pendingDebugView = view;
    m_pendingDebugProj = proj;
    m_pendingDebugLinesColored.clear();
    const math::Vec4 color(r, g, b, 1.f);
    m_pendingDebugLinesColored.reserve(lines.size());
    for (const auto& line : lines)
        m_pendingDebugLinesColored.push_back({ line, color });
}

void RenderPipeline::SetPendingDebugLinesColored(const math::Mat4& view, const math::Mat4& proj,
    const std::vector<std::pair<std::pair<math::Vec3, math::Vec3>, math::Vec4>>& linesWithColors) {
    m_pendingDebugView = view;
    m_pendingDebugProj = proj;
    m_pendingDebugLinesColored = linesWithColors;
}

void RenderPipeline::ReloadShaders() {
    if (m_skyboxShader) m_skyboxShader->Reload();
    if (m_outlinePostShader) m_outlinePostShader->Reload();
    if (m_entityIdShader) m_entityIdShader->Reload();
    if (m_forwardPlusShader) m_forwardPlusShader->Reload();
    if (m_forwardPlusSkinnedShader) m_forwardPlusSkinnedShader->Reload();
    if (m_stereoForwardShader) m_stereoForwardShader->Reload();
    if (m_gbufferPass) m_gbufferPass->ReloadShaders();
    if (m_shadowPass) m_shadowPass->ReloadShaders();
    if (m_lightingPass) m_lightingPass->ReloadShaders();
    if (m_postProcessor) m_postProcessor->ReloadShaders();
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
