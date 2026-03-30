#include "eyelib/render/renderer/RenderPipeline.h"
#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/GpuProfilerRHI.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/lighting/ClusteredLightGrid.h"
#include "eyelib/render/post/PostProcessor.h"
#include "eyelib/render/ibl/IBL.h"
#include "eyelib/render/reflection/ReflectionProbeManager.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/shadows/ShadowPass.h"
#include "eyelib/render/passes/LightingPass.h"
#include "eyelib/render/passes/TransparencyPass.h"
#include "eyelib/render/passes/SkyboxPassSystem.h"
#include "eyelib/render/passes/PostProcessPassSystem.h"
#include "eyelib/render/passes/TransparencyPassSystem.h"
#include "eyelib/render/passes/GBufferPassSystem.h"
#include "eyelib/render/passes/DeferredLightingPassSystem.h"
#include "eyelib/render/passes/ForwardOpaquePassSystem.h"
#include "eyelib/render/passes/ForwardGeometryCommon.h"
#include "eyelib/render/passes/detail/LightCullPass.h"
#include "eyelib/render/passes/ShadowPassSystem.h"
#include "eyelib/render/passes/EditorOverlayPassSystem.h"
#include "eyelib/render/passes/StreamingPassSystem.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/render/lighting/SceneLighting.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/render/DebugDrawRenderer.h"
#include "eyelib/render/RenderContext.h"
#include "eyelib/render/2d/Primitives.h"
#include "eyelib/debug/Presets.h"
#include "eyelib/debug/Profiler.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/submission/DrawBatch.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/render/resources/PassResourceBuilders.h"
#include "eyelib/render/passes/common/ForwardSharedEnvironment.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/TextureCube.h"
#include "eyelib/core/Log.h"
#include "eyelib/core/Time.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/PipelineAttachmentFormatSets.h"
#include <optional>
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/render/rhi/Vulkan/commands/CommandListVK.h"
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/renderer/graph/FrameGraphRecipeBuilders.h"
#include "eyelib/render/renderer/graph/RenderGraphRecipes.h"

#include "eyelib/math/TransformUtils.h"
#include "eyelib/math/Frustum.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <glm/gtc/matrix_inverse.hpp>
#include <span>
#include <cstring>
#include <string>

namespace eyelib {
namespace render {

namespace {
constexpr uint32_t kDefaultDirectionalShadowResolution = 1536u;
constexpr uint32_t kDefaultDirectionalShadowCascades = 3u;

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
    m_reflectionProbeManager.reset();
    m_ibl.reset();
    m_iblEnvironmentTexture = nullptr;
    m_stereoForwardShader.reset();
    m_outlinePostShader.reset();
    m_entityIdShader.reset();
    if (m_device) {
        if (m_streamingNV12Pipeline) { m_device->DestroyPipeline(m_streamingNV12Pipeline); m_streamingNV12Pipeline = nullptr; }
        if (m_streamingNV12Shader) { m_device->DestroyShader(m_streamingNV12Shader); m_streamingNV12Shader = nullptr; }
        if (m_outlineCompositeTemp) { m_device->DestroyRenderTarget(m_outlineCompositeTemp); m_outlineCompositeTemp = nullptr; }
        if (m_entityIdRenderTarget) { m_device->DestroyRenderTarget(m_entityIdRenderTarget); m_entityIdRenderTarget = nullptr; }
    }
    m_clusteredLightGrid.reset();
    m_renderGraphOrchestrator.reset();
    if (m_deferredLightingPassSystem && m_device)
        m_deferredLightingPassSystem->Shutdown(m_device);
    m_deferredLightingPassSystem.reset();
    m_lightingPass.reset();
    if (m_shadowPass && m_device) m_shadowPass->Shutdown();
    m_shadowPass.reset();
    m_gpuDrivenQuad.reset();
    if (m_lightCullPassSystem && m_device)
        m_lightCullPassSystem->Shutdown();
    m_lightCullPassSystem.reset();
    if (m_device) {
        if (m_passResourceBuilder) m_passResourceBuilder->Shutdown(m_device);
        m_frameSet = nullptr;
        m_passSet = nullptr;
        m_transparentPassSet = nullptr;
        m_materialSet = nullptr;
        m_sceneSet = nullptr;
        m_transparentSceneSet = nullptr;
        m_deferredLightingSet = nullptr;
        if (m_skyboxPassSystem) {
            m_skyboxPassSystem->Shutdown();
            m_skyboxPassSystem.reset();
        }
        for (rhi::IDescriptorSet*& s : m_outlinePostSets) {
            if (s) { m_device->EnqueueDestroyDescriptorSet(s); s = nullptr; }
        }
        m_frameUbo = nullptr;
        m_bonesBuffer = nullptr;
    }
    m_passResourceBuilder.reset();
    if (m_gpuCullPassSystem && m_device)
        m_gpuCullPassSystem->Shutdown();
    m_gpuCullPassSystem.reset();
    if (m_gbufferPassSystem && m_device)
        m_gbufferPassSystem->Shutdown(m_device);
    m_gbufferPassSystem.reset();
    m_shaderCache = nullptr;
    m_usingShared = nullptr;
    m_sharedResources.reset();
    m_cascadeShadowMap.reset();
    m_gbuffer.reset();
    if (m_device) {
        if (m_bindlessHeap) { m_device->DestroyBindlessHeap(m_bindlessHeap); m_bindlessHeap = nullptr; }
        m_bindlessHandleCache.clear();
        if (m_instanceMatrixBuffer) { m_device->DestroyBuffer(m_instanceMatrixBuffer); m_instanceMatrixBuffer = nullptr; }
        if (m_prevFrameDepthRT) { m_device->DestroyRenderTarget(m_prevFrameDepthRT); m_prevFrameDepthRT = nullptr; }
        if (m_pointLightBuffer) { m_device->DestroyBuffer(m_pointLightBuffer); m_pointLightBuffer = nullptr; }
        if (m_spotLightBuffer) { m_device->DestroyBuffer(m_spotLightBuffer); m_spotLightBuffer = nullptr; }
        if (m_lightIndexBuffer) { m_device->DestroyBuffer(m_lightIndexBuffer); m_lightIndexBuffer = nullptr; }
        if (m_sceneLightingBuffer) { m_device->DestroyBuffer(m_sceneLightingBuffer); m_sceneLightingBuffer = nullptr; }
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
    return InitializeDeviceResources(width, height, nullptr);
}

bool RenderPipeline::Initialize(rhi::IDevice* sharedDevice, uint32_t width, uint32_t height) {
    if (!sharedDevice) {
        core::Log::Error("RenderPipeline::Initialize(shared): device is null");
        return false;
    }
    m_width = width;
    m_height = height;
    m_device = sharedDevice;
    return InitializeDeviceResources(width, height, nullptr);
}

bool RenderPipeline::Initialize(rhi::IDevice* sharedDevice, uint32_t width, uint32_t height, PipelineSharedResources* shareFrom) {
    if (!sharedDevice) {
        core::Log::Error("RenderPipeline::Initialize(shared, shareFrom): device is null");
        return false;
    }
    if (!shareFrom || !shareFrom->shaderCache) {
        core::Log::Error("RenderPipeline::Initialize(shareFrom): shareFrom or shaderCache is null");
        return false;
    }
    m_width = width;
    m_height = height;
    m_device = sharedDevice;
    m_usingShared = shareFrom;
    return InitializeDeviceResources(width, height, shareFrom);
}

bool RenderPipeline::InitializeDeviceResources(uint32_t width, uint32_t height, PipelineSharedResources* shareFrom) {
    Primitives::SetStaticDevice(m_device);
    if (!shareFrom) {
        m_sharedResources = std::make_unique<PipelineSharedResources>();
        m_sharedResources->shaderCache = std::make_unique<ShaderCache>();
    }
    m_shaderCache = shareFrom ? shareFrom->shaderCache.get() : m_sharedResources->shaderCache.get();

    m_gbuffer = std::make_unique<GBuffer>();
    if (!m_gbuffer->Initialize(m_device, width, height)) {
        core::Log::Error("RenderPipeline::Initialize: GBuffer::Initialize failed");
        return false;
    }

    m_cascadeShadowMap = std::make_unique<CascadeShadowMap>();
    if (!m_cascadeShadowMap->Initialize(m_device, kDefaultDirectionalShadowResolution, kDefaultDirectionalShadowCascades)) {
        core::Log::Error("RenderPipeline::Initialize: CascadeShadowMap::Initialize failed");
        return false;
    }

    m_gbufferPassSystem = std::make_unique<GBufferPassSystem>();
    m_gbufferPassSystem->SetShaderCache(m_shaderCache);
    if (backend::SupportsBindless(m_device))
        m_bindlessHeap = m_device->CreateBindlessHeap();
    if (m_bindlessHeap)
        m_gbufferPassSystem->SetBindlessHeap(m_bindlessHeap);
    m_gbufferPassSystem->SetBindlessGetHandle([this](rhi::ITexture* t) { return GetOrAllocateBindlessHandle(t); });
    m_gpuCullPassSystem = std::make_unique<GpuCullPassSystem>();
    if (!m_gpuCullPassSystem->Initialize(m_device)) {
        core::Log::Error("RenderPipeline::Initialize: GpuCullPassSystem::Initialize failed");
        return false;
    }
    m_lightCullPassSystem = std::make_unique<LightCullPassSystem>();
    if (!m_lightCullPassSystem->Initialize(m_device)) {
        core::Log::Error("RenderPipeline::Initialize: LightCullPassSystem::Initialize failed");
        return false;
    }
    if (m_device) {
        rhi::BufferDesc bd;
        bd.sizeInBytes = kMaxInstanceBatchSize * sizeof(math::Mat4);
        bd.usage = rhi::BufferUsage::Storage;
        bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_instanceMatrixBuffer = m_device->CreateBuffer(bd, nullptr);
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
    if (m_shadowPass && m_shaderCache)
        m_shadowPass->SetShaderCache(m_shaderCache);
    m_lightingPass = std::make_unique<LightingPass>();
    // Lighting pass owns its shader: created on first Render() via EnsureDefaultShader(device) with correct layout kind.
    {
        rhi::BufferDesc bd;
        bd.sizeInBytes = kSceneLightingBufferSize;
        bd.usage = rhi::BufferUsage::Storage;
        bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_sceneLightingBuffer = m_device->CreateBuffer(bd, nullptr);
    }

    m_renderGraphOrchestrator = std::make_unique<RenderGraphOrchestrator>();
    m_renderGraphOrchestrator->SetFrameSize(width, height);
    m_clusteredLightGrid = std::make_unique<ClusteredLightGrid>();
    m_clusteredLightGrid->Initialize(width, height, 0.1f, 1000.0f);  // Default, will be updated per-frame

    // Debug: set to false to enable post-processing and narrow skybox-vs-post bug.
    static constexpr bool kDisablePostForDebug = true;
    if (!kDisablePostForDebug) {
        m_postProcessor = std::make_unique<PostProcessor>();
        if (!m_postProcessor->Initialize(width, height, m_device)) {
            core::Log::Error("RenderPipeline::Initialize: PostProcessor::Initialize failed");
            return false;
        }
    }
    m_reflectionProbeManager = std::make_unique<ReflectionProbeManager>();

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

    m_skyboxPassSystem = std::make_unique<SkyboxPassSystem>();
    m_skyboxPassSystem->SetDevice(m_device);
    m_skyboxPassSystem->SetSharedResources(m_sharedResources.get(), m_usingShared);

    WarmupPipelines();

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
    if (m_renderGraphOrchestrator)
        m_renderGraphOrchestrator->MarkNeedsCompile();
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
    if (m_renderGraphOrchestrator)
        m_renderGraphOrchestrator->SetFrameSize(renderW, renderH);
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

void RenderPipeline::UpdateUploads() {
    // Dedicated upload phase: CPU asset updates → staging (UploadHeap) → vkCmdCopyBuffer / vkCmdCopyBufferToImage.
    // All uploads must happen here so they are deterministic and before CompileFrameGraph/Execute. Currently a no-op;
    // move texture/mesh staging uploads from creation paths into this phase as they are refactored.
    (void)m_device;
}

void RenderPipeline::WarmupPipelines() {
    if (!m_device || !m_shaderCache) return;
    if (m_skyboxPassSystem) m_skyboxPassSystem->EnsureShaders();
    EnsureStereoForwardShader();
    EnsureOutlinePostShader();
    EnsureEntityIdShader();
    ShaderKey key;
    key.stage = PipelineStage::GBuffer;
    key.instanceType = InstanceType::Static;
    key.materialTypeId = kDefaultMaterialTypeId;
    m_shaderCache->getOrCompile(m_device, key);
    key.stage = PipelineStage::Forward;
    m_shaderCache->getOrCompile(m_device, key);
    key.stage = PipelineStage::ForwardPlus;
    m_shaderCache->getOrCompile(m_device, key);
    key.instanceType = InstanceType::Skinned;
    key.stage = PipelineStage::GBuffer;
    m_shaderCache->getOrCompile(m_device, key);
    key.stage = PipelineStage::Forward;
    m_shaderCache->getOrCompile(m_device, key);
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
    if (!texture || !m_bindlessHeap || !backend::SupportsBindless(m_device))
        return rhi::kInvalidBindlessHandle;
    auto it = m_bindlessHandleCache.find(texture);
    if (it != m_bindlessHandleCache.end())
        return it->second;
    rhi::BindlessHandle h = m_bindlessHeap->AllocateTextureHandle(texture);
    if (h != rhi::kInvalidBindlessHandle)
        m_bindlessHandleCache[texture] = h;
    return h;
}

/** Enable CPU frustum culling once CPU uses raw (unadjusted) camera clip-space math. */
constexpr bool kEnableCpuFrustumCulling = true;

void RenderPipeline::PrepareFrameData(const IRenderSceneView& view) {
    m_frameData.cameraRaw = view.GetCamera();
    m_frameData.camera = backend::AdjustCameraForBackend(m_device, m_frameData.cameraRaw);
    m_frameData.light = view.GetLight();
    m_frameData.skybox = view.GetSkybox();
    m_frameData.pointLights.clear();
    m_frameData.spotLights.clear();
    view.ForEachPointLight([this](const PointLight& p) { m_frameData.pointLights.push_back(p); });
    view.ForEachSpotLight([this](const SpotLight& s) { m_frameData.spotLights.push_back(s); });

    m_frameData.allOpaque.clear();
    m_frameData.visibleOpaque.clear();
    if constexpr (kEnableCpuFrustumCulling) {
        math::Frustum cameraFrustum;
        // CPU culling uses source camera conventions. Backend-adjusted projection is for GPU clip-space only.
        cameraFrustum.FromMatrix(m_frameData.cameraRaw.projection * m_frameData.cameraRaw.view);
        view.ForEachOpaqueDrawable([this, &cameraFrustum](const Drawable& d) {
            m_frameData.allOpaque.push_back(d);
            if (!d.mesh) return;
            if (d.bones && d.boneCount > 0) {
                m_frameData.visibleOpaque.push_back(d);
                return;
            }
            const math::AABB worldAabb = d.mesh->GetAABB().Transform(d.transform);
            if (cameraFrustum.Contains(worldAabb) != 0)
                m_frameData.visibleOpaque.push_back(d);
        });
    } else {
        view.ForEachOpaqueDrawable([this](const Drawable& d) {
            m_frameData.allOpaque.push_back(d);
            if (!d.mesh) return;
            m_frameData.visibleOpaque.push_back(d);
        });
    }

    m_frameData.visibleTransparent.clear();
    view.ForEachTransparentDrawable([this](const Drawable& d) { m_frameData.visibleTransparent.push_back(d); });

}

GpuCullScenePrepUploadParams RenderPipeline::BuildGpuCullScenePrepUploadParams(const CameraView& cam) const {
    GpuCullScenePrepUploadParams u{};
    if (m_gpuCullPassSystem) {
        u.gpuCullPass = m_gpuCullPassSystem->GetPass();
        u.transformBuffer = m_gpuCullPassSystem->GetTransformBuffer();
    }
    u.device = m_device;
    u.width = m_width;
    u.height = m_height;
    u.havePrevDepth = m_havePrevDepth;
    u.prevFrameDepthRT = m_prevFrameDepthRT;
    u.cam = cam;
    return u;
}

uint32_t RenderPipeline::PrepareGpuCullFromFrameData(const RenderFrameData& frameData, const CameraView& cam) {
    return m_gpuCullPassSystem->PrepareFromVisibleOpaque(BuildGpuCullScenePrepUploadParams(cam), frameData.visibleOpaque);
}

IBL* RenderPipeline::EnsureIBLFromFrameData(const RenderFrameData& frameData) {
    if (!frameData.skybox || !frameData.skybox->IsValid()) {
        m_iblEnvironmentTexture = nullptr;
        return nullptr;
    }
    rhi::ITexture* skyTex = frameData.skybox->GetTextureRHI();
    if (!skyTex) return nullptr;
    if (!m_ibl) {
        m_ibl = std::make_unique<IBL>();
        m_ibl->SetDevice(m_device);
    }
    if (m_iblEnvironmentTexture != skyTex) {
        m_ibl->Initialize(skyTex, 32, 512);
        // Record attempt so we do not re-run Initialize every frame when shader/asset init fails (was causing shaderc spam + hitch).
        m_iblEnvironmentTexture = skyTex;
    }
    return m_ibl->IsValid() ? m_ibl.get() : nullptr;
}

void RenderPipeline::PreparePersistentShaderReadInputs(rhi::ICommandList* cmd, const RenderFrameData& frameData) {
    if (!cmd) return;
    if (frameData.skybox && frameData.skybox->IsValid()) {
        if (rhi::ITexture* sky = frameData.skybox->GetTextureRHI())
            rhi::BarrierTextureForShaderRead(cmd, sky);
    }
    IBL* iblPtr = EnsureIBLFromFrameData(frameData);
    const bool useTypedForwardPush = backend::UseTypedPushConstants(m_device);
    if (iblPtr && iblPtr->IsValid()) {
        if (rhi::ITexture* ir = iblPtr->GetIrradianceTextureRHI()) rhi::BarrierTextureForShaderRead(cmd, ir);
        if (rhi::ITexture* pf = iblPtr->GetPrefilterTextureRHI()) rhi::BarrierTextureForShaderRead(cmd, pf);
        if (rhi::ITexture* brdf = iblPtr->GetBRDFLUTTextureRHI()) rhi::BarrierTextureForShaderRead(cmd, brdf);
    }
    const ReflectionProbe* envProbe = passes::common::ResolveBestReflectionProbe(m_reflectionProbeManager.get(), frameData.camera.viewPos);
    if (envProbe && envProbe->IsValid()) {
        if (rhi::ITexture* ir = envProbe->GetIrradianceMap()) rhi::BarrierTextureForShaderRead(cmd, ir);
        if (rhi::ITexture* pf = envProbe->GetPrefilteredMap()) rhi::BarrierTextureForShaderRead(cmd, pf);
    }
}

uint32_t RenderPipeline::PrepareGpuCullFromView(const IRenderSceneView& view, const CameraView& cam) {
    return m_gpuCullPassSystem->PrepareFromView(BuildGpuCullScenePrepUploadParams(cam), view);
}

GBufferPassOpaqueParams RenderPipeline::BuildGBufferOpaqueParams(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam,
                                                                 uint32_t gpuCullDrawListCount) {
    GBufferPassOpaqueParams p{};
    p.device = m_device;
    p.cmd = cmd;
    p.gbuffer = m_gbuffer.get();
    p.ensureCurrentFrameResourceSets = [this]() { EnsureCurrentFrameResourceSets(); };
    p.frameUbo = m_frameUbo;
    p.prevView = m_prevView;
    p.prevProjection = m_prevProjection;
    p.useGpuDrivenGBuffer = m_useGpuDrivenGBuffer;
    p.gpuCullPass = m_gpuCullPassSystem ? m_gpuCullPassSystem->GetPass() : nullptr;
    p.gpuDrivenQuad = m_gpuDrivenQuad ? m_gpuDrivenQuad.get() : nullptr;
    p.gpuCullSingleMesh = m_gpuCullPassSystem ? m_gpuCullPassSystem->Scratch().singleMesh : false;
    p.gpuCullRepresentativeMesh = m_gpuCullPassSystem ? m_gpuCullPassSystem->Scratch().representativeMesh : nullptr;
    p.gpuCullTransformBuffer = m_gpuCullPassSystem ? m_gpuCullPassSystem->GetTransformBuffer() : nullptr;
    p.instanceMatrixBuffer = m_instanceMatrixBuffer;
    p.frameData = &frameData;
    p.cam = cam;
    p.gpuCullDrawListCount = gpuCullDrawListCount;
    return p;
}

ShadowPassBatchedParams RenderPipeline::BuildShadowBatchedParams(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& camView) const {
    ShadowPassBatchedParams p{};
    p.device = m_device;
    p.cmd = cmd;
    p.shadowPass = m_shadowPass.get();
    p.cascadeShadowMap = m_cascadeShadowMap.get();
    p.instanceMatrixBuffer = m_instanceMatrixBuffer;
    p.frameData = &frameData;
    p.camView = camView;
    p.maxDirectionalShadowDistance = kMaxDirectionalShadowDrawDistance;
    return p;
}

ShadowPassBatchedParams RenderPipeline::BuildShadowBatchedParams(const ShadowPassBuildParams& build) const {
    if (!build.frameData) return {};
    return BuildShadowBatchedParams(build.cmd, *build.frameData, build.camView);
}

ShadowPassBuildParams RenderPipeline::BuildShadowPassParams(const FrameRenderContext& frameCtx, const CameraView& camView) {
    ShadowPassBuildParams build{};
    build.cmd = frameCtx.cmd;
    const RenderFrameData* frameData = frameCtx.frameData;
    build.frameData = frameData;
    build.camView = camView;
    build.enablePass = (frameData && frameData->light.castShadows);
    return build;
}

RenderPipeline::DeferredLightingPassParams RenderPipeline::BuildDeferredLightingPassParams(const FrameRenderContext& frameCtx, const CameraView& camView,
                                                                                           const rhi::ClearValue& clearColor) {
    DeferredLightingPassParams p{};
    p.cmd = frameCtx.cmd;
    const RenderFrameData* frameData = frameCtx.frameData;
    p.frameData = frameData;
    p.camView = camView;
    p.clearColor = clearColor;
    if (!frameData || !m_lightingPass || !m_gbuffer)
        return p;

    EnsureCurrentFrameResourceSets();
    p.lightParams = frameData->light;
    p.ibl = EnsureIBLFromFrameData(*frameData);
    p.cascadeForLighting = p.lightParams.castShadows ? m_cascadeShadowMap.get() : nullptr;

    math::Mat4 lightSpaceMatrix(1.0f);
    if (p.cascadeForLighting && p.cascadeForLighting->GetSplitCount() > 0)
        lightSpaceMatrix = p.cascadeForLighting->GetLightSpaceMatrix(0);

    FillSceneLightingBuffer(m_sceneLightingBuffer, p.lightParams, camView.viewPos, lightSpaceMatrix, 0.005f,
                            p.lightParams.castShadows, &frameData->pointLights, &frameData->spotLights, p.cascadeForLighting);

    p.lightingSetForCall = AcquireDeferredLightingSet();
    if (!p.lightingSetForCall) {
        static bool s_loggedMissingDeferredSet = false;
        if (!s_loggedMissingDeferredSet) {
            core::Log::Warn("RenderPipeline::BuildDeferredLightingPassParams: AcquireDeferredLightingSet returned null.");
            s_loggedMissingDeferredSet = true;
        }
    }
    return p;
}

RenderPipeline::TransparencyPassParams RenderPipeline::BuildTransparencyPassParams(const FrameRenderContext& frameCtx, const CameraView& camView) {
    TransparencyPassParams p{};
    p.device = m_device;
    p.cmd = frameCtx.cmd;
    p.hdrRenderTarget = m_hdrRenderTarget;
    p.gbufferRenderTarget = m_gbuffer ? m_gbuffer->GetRenderTargetRHI() : nullptr;
    p.width = frameCtx.width;
    p.height = frameCtx.height;
    const RenderFrameData* frameData = frameCtx.frameData;
    p.frameData = frameData;
    p.camView = camView;
    p.copyDepthFromGBuffer = (m_mode == PipelineMode::Deferred);
    p.preferVulkanPath = backend::PreferVulkanTransparencyPath(m_device, m_shaderCache);
    return p;
}

RenderPipeline::PostProcessPassParams RenderPipeline::BuildPostProcessPassParams(const FrameRenderContext& frameCtx, const IRenderSceneView* sceneView,
                                                                                  const CameraView& camView) {
    PostProcessPassParams p{};
    p.sceneView = sceneView;
    p.camView = camView;
    const RenderFrameData* frameData = frameCtx.frameData;
    p.ibl = frameData ? EnsureIBLFromFrameData(*frameData) : nullptr;
    if (p.ibl && p.ibl->IsValid() && m_reflectionProbeManager)
        m_reflectionProbeManager->SetGlobalProbeFromPrefiltered(p.ibl->GetPrefilterTextureRHI(), p.ibl->GetIrradianceTextureRHI());
    if (m_reflectionProbeManager) {
        const ReflectionProbe* probe = passes::common::ResolveBestReflectionProbe(m_reflectionProbeManager.get(), camView.viewPos);
        if (probe && probe->IsValid()) p.probeTex = probe->GetCubemapTexture();
    }
    p.invView = glm::inverse(camView.view);
    p.invProj = glm::inverse(camView.projection);
    auto skybox = sceneView ? sceneView->GetSkybox() : nullptr;
    p.skyboxTex = skybox && skybox->IsValid() ? skybox->GetTextureRHI() : nullptr;
    p.time = core::Time::TotalTime();
    p.hdrTex = GetHDRColorTextureRHI();
    p.ssrNearPlane = camView.nearPlane;
    p.ssrFarPlane = camView.farPlane;
    return p;
}

void RenderPipeline::ExecuteShadowPass(const ShadowPassBuildParams& buildParams) {
    if (!buildParams.frameData || !buildParams.enablePass) return;
    if (!m_shadowPassSystem) m_shadowPassSystem = std::make_unique<ShadowPassSystem>();
    m_shadowPassSystem->RenderShadowBatched(BuildShadowBatchedParams(buildParams));
}

void RenderPipeline::ExecuteDeferredLightingPass(const DeferredLightingPassParams& params) {
    if (!params.frameData || !m_lightingPass || !m_gbuffer) return;
    if (!m_deferredLightingPassSystem) m_deferredLightingPassSystem = std::make_unique<DeferredLightingPassSystem>();
    DeferredLightingPassBuildParams build{};
    build.device = m_device;
    build.cmd = params.cmd;
    build.frameData = params.frameData;
    build.camView = params.camView;
    build.clearColor = params.clearColor;
    build.lightParams = params.lightParams;
    build.cascadeForLighting = params.cascadeForLighting;
    build.ibl = params.ibl;
    build.sceneLightingBuffer = m_sceneLightingBuffer;
    build.frameUbo = m_frameUbo;
    build.prevView = m_prevView;
    build.prevProjection = m_prevProjection;
    build.hdrRenderTarget = m_hdrRenderTarget;
    build.width = m_width;
    build.height = m_height;
    build.gbuffer = m_gbuffer.get();
    build.frameSet = m_frameSet;
    build.deferredLightingSet = params.lightingSetForCall;
    build.lightingPass = m_lightingPass.get();
    DeferredLightingPassExecuteParams execParams = m_deferredLightingPassSystem->BuildParams(build);
    (void)m_deferredLightingPassSystem->Execute(execParams);
}

void RenderPipeline::ExecuteTransparencyPass(const TransparencyPassParams& params) {
    if (!params.frameData) return;
    if (!m_transparencyPassSystem) m_transparencyPassSystem = std::make_unique<TransparencyPassSystem>();
    TransparencyPassBuildParams build{};
    build.device = params.device;
    build.cmd = params.cmd;
    build.hdrRenderTarget = params.hdrRenderTarget;
    build.gbufferRenderTarget = params.gbufferRenderTarget;
    build.width = params.width;
    build.height = params.height;
    build.frameData = params.frameData;
    build.camView = params.camView;
    build.preferVulkanPath = params.preferVulkanPath;
    build.shaderCache = m_shaderCache;
    build.drawVulkan = [this](rhi::ICommandList* cmd, const RenderFrameData& fd, const CameraView& cv) {
        RenderForwardTransparent(cmd, fd, cv);
    };
    build.drawLegacy = [](rhi::IDevice* device, rhi::ICommandList* cmd, const std::vector<Drawable>& transparent,
                          const CameraView& cv, ShaderCache* shaderCache) {
        RenderContext c{};
        c.device = device;
        c.cmd = cmd;
        TransparencyPass::Render(c, transparent, cv.view, cv.projection, cv.viewPos, shaderCache);
    };
    TransparencyPassExecuteParams execParams = m_transparencyPassSystem->BuildParams(build);
    (void)m_transparencyPassSystem->Execute(execParams);
}

void RenderPipeline::ExecutePostProcessPass(const PostProcessPassParams& params) {
    if (!m_postProcessor || !m_gbuffer) return;
    if (!m_postProcessPassSystem) m_postProcessPassSystem = std::make_unique<PostProcessPassSystem>();
    PostProcessPassBuildParams build{};
    build.postProcessor = m_postProcessor.get();
    build.gbuffer = m_gbuffer.get();
    build.hdrTex = params.hdrTex;
    build.skyboxTex = params.skyboxTex;
    build.probeTex = params.probeTex;
    build.camView = params.camView;
    build.invView = params.invView;
    build.invProj = params.invProj;
    build.time = params.time;
    build.ssrNearPlane = params.ssrNearPlane;
    build.ssrFarPlane = params.ssrFarPlane;
    PostProcessPassExecuteParams executeParams = m_postProcessPassSystem->BuildParams(build);
    (void)m_postProcessPassSystem->Execute(executeParams);
}

FrameRenderContext RenderPipeline::GetFrameRenderContext() const {
    FrameRenderContext ctx{};
    ctx.device = m_device;
    ctx.cmd = m_device ? m_device->GetImmediateCommandList() : nullptr;
    ctx.frameData = m_currentFrameData;
    ctx.width = m_width;
    ctx.height = m_height;
    if (m_device) ctx.frameIndex = m_device->GetCurrentFrameIndex();
    return ctx;
}

void RenderPipeline::BuildAndExecuteFrameGraph(const IRenderSceneView& view) {
    if (!m_renderGraphOrchestrator || !m_gbuffer || !m_cascadeShadowMap || !m_gbufferPassSystem || !m_shadowPass || !m_lightingPass) return;

    m_currentView = &view;
    PrepareFrameData(view);
    m_currentFrameData = &m_frameData;
    debug::Profiler::SetFrameCounter("visibleMeshes",
        static_cast<uint64_t>(m_frameData.visibleOpaque.size() + m_frameData.visibleTransparent.size()));
    const FrameRenderContext frc = GetFrameRenderContext();
    RenderContext ctx = MakeRenderContext(frc);
    if (ctx.cmd) ctx.cmd->SetRenderTarget(nullptr);
    m_lastGpuCullDrawListCount = PrepareGpuCullFromFrameData(m_frameData, m_frameData.camera);
    PreparePersistentShaderReadInputs(frc.cmd, m_frameData);

    CompileFrameGraphIfNeeded(PipelineMode::Deferred, false);
    m_frameGraphExecutionContext = frc;
    m_renderGraphOrchestrator->Execute(ctx);
    m_frameGraphExecutionContext = {};
}

void RenderPipeline::BuildAndExecuteForwardFrameGraph(const IRenderSceneView& view) {
    if (!m_renderGraphOrchestrator || !m_cascadeShadowMap || !m_shadowPass || !m_hdrRenderTarget) return;

    m_currentView = &view;
    PrepareFrameData(view);
    m_currentFrameData = &m_frameData;
    debug::Profiler::SetFrameCounter("visibleMeshes",
        static_cast<uint64_t>(m_frameData.visibleOpaque.size() + m_frameData.visibleTransparent.size()));
    const FrameRenderContext frc = GetFrameRenderContext();
    RenderContext ctx = MakeRenderContext(frc);
    if (ctx.cmd) ctx.cmd->SetRenderTarget(nullptr);
    m_lastGpuCullDrawListCount = PrepareGpuCullFromFrameData(m_frameData, m_frameData.camera);
    PreparePersistentShaderReadInputs(frc.cmd, m_frameData);

    CompileFrameGraphIfNeeded(PipelineMode::Forward, false);
    m_frameGraphExecutionContext = frc;
    m_renderGraphOrchestrator->Execute(ctx);
    m_frameGraphExecutionContext = {};
}

void RenderPipeline::BuildAndExecuteForwardPlusFrameGraph(const IRenderSceneView& view) {
    if (!m_renderGraphOrchestrator || !m_cascadeShadowMap || !m_shadowPass || !m_hdrRenderTarget || !m_clusteredLightGrid) return;

    m_currentView = &view;
    PrepareFrameData(view);
    m_currentFrameData = &m_frameData;
    debug::Profiler::SetFrameCounter("visibleMeshes",
        static_cast<uint64_t>(m_frameData.visibleOpaque.size() + m_frameData.visibleTransparent.size()));
    const FrameRenderContext frc = GetFrameRenderContext();
    RenderContext ctx = MakeRenderContext(frc);
    if (ctx.cmd) ctx.cmd->SetRenderTarget(nullptr);
    PreparePersistentShaderReadInputs(frc.cmd, m_frameData);

    CompileFrameGraphIfNeeded(PipelineMode::ForwardPlus, false);
    m_frameGraphExecutionContext = frc;
    m_renderGraphOrchestrator->Execute(ctx);
    m_frameGraphExecutionContext = {};
}

void RenderPipeline::EnsureCurrentFrameResourceSets() {
    if (!m_device) return;
    if (!m_passResourceBuilder) m_passResourceBuilder = std::make_unique<resources::PassResourceBuilder>();
    if (!m_passResourceBuilder->Ensure(m_device)) return;
    const auto& current = m_passResourceBuilder->Current();
    m_frameSet = current.frameSet;
    m_passSet = current.passSet;
    m_transparentPassSet = current.transparentPassSet;
    m_materialSet = current.materialSet;
    m_sceneSet = current.sceneSet;
    m_transparentSceneSet = current.transparentSceneSet;
    m_deferredLightingSet = current.deferredLightingSet;
    m_frameUbo = current.frameUbo;
    m_bonesBuffer = current.bonesBuffer;
    if (m_gbufferPassSystem) {
        m_gbufferPassSystem->SetResourceSpaceSets(m_frameSet, m_materialSet, m_sceneSet);
        if (m_bonesBuffer) m_gbufferPassSystem->SetBonesBuffer(m_bonesBuffer);
    }
    if (m_lightingPass)
        m_lightingPass->SetResourceSpaceSets(m_frameSet, m_deferredLightingSet);
}

rhi::IDescriptorSet* RenderPipeline::AcquireMaterialDrawSet() {
    if (!m_device || !m_passResourceBuilder) return nullptr;
    return m_passResourceBuilder->AcquireMaterialDrawSet(m_device);
}

rhi::IDescriptorSet* RenderPipeline::AcquirePassDrawSet() {
    if (!m_device || !m_passResourceBuilder) return nullptr;
    return m_passResourceBuilder->AcquirePassDrawSet(m_device);
}

rhi::IDescriptorSet* RenderPipeline::AcquireSceneDrawSet() {
    if (!m_device || !m_passResourceBuilder) return nullptr;
    return m_passResourceBuilder->AcquireSceneDrawSet(m_device);
}

rhi::IDescriptorSet* RenderPipeline::AcquireDeferredLightingSet() {
    if (!m_device || !m_passResourceBuilder) return nullptr;
    return m_passResourceBuilder->AcquireDeferredLightingSet(m_device);
}

void RenderPipeline::BuildForwardPassSharedState(const RenderFrameData& frameData, const CameraView& cam, rhi::IDescriptorSet* passSet,
                                                 rhi::IDescriptorSet* sceneBuffersSet, bool usePrevViewProj) {
    if (!passSet || !sceneBuffersSet || !m_frameUbo) return;
    const LightParams& light = frameData.light;
    IBL* iblPtr = EnsureIBLFromFrameData(frameData);
    const passes::common::ForwardSharedEnvironmentState envState =
        passes::common::ResolveForwardSharedEnvironmentForPipeline(frameData, cam, m_cascadeShadowMap.get(), m_reflectionProbeManager.get(), iblPtr);
    passes::common::ForwardSharedStateWriteParams writeParams{};
    writeParams.passSet = passSet;
    writeParams.sceneSet = sceneBuffersSet;
    writeParams.frameUbo = m_frameUbo;
    writeParams.sceneLightingBuffer = m_sceneLightingBuffer;
    writeParams.lightIndexBuffer = m_lightIndexBuffer;
    writeParams.bonesBuffer = m_bonesBuffer;
    writeParams.light = &light;
    writeParams.camera = &cam;
    writeParams.pointLights = &frameData.pointLights;
    writeParams.spotLights = &frameData.spotLights;
    writeParams.prevView = &m_prevView;
    writeParams.prevProjection = &m_prevProjection;
    writeParams.lightSpaceMatrix = &envState.lightSpaceMatrix;
    writeParams.cascadeShadowMap = m_cascadeShadowMap.get();
    writeParams.shadowDepthTexture = envState.shadowDepthTexture;
    writeParams.irradianceTexture = envState.irradianceTexture;
    writeParams.prefilterTexture = envState.prefilterTexture;
    writeParams.brdfLutTexture = envState.brdfLutTexture;
    writeParams.useShadows = envState.useShadows;
    writeParams.usePrevViewProj = usePrevViewProj;
    (void)passes::common::WriteForwardSharedState(writeParams);
}

void RenderPipeline::RenderForwardOpaqueForCamera(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam,
                                                  const std::vector<Drawable>& opaqueDrawables) {
    ForwardOpaqueRenderContext ctx{};
    ctx.device = m_device;
    ctx.shaderCache = m_shaderCache;
    ctx.passResourceBuilder = m_passResourceBuilder.get();
    ctx.frameUbo = m_frameUbo;
    ctx.bonesBuffer = m_bonesBuffer;
    ctx.sceneLightingBuffer = m_sceneLightingBuffer;
    ctx.lightIndexBuffer = m_lightIndexBuffer;
    ctx.bindlessHeap = m_bindlessHeap;
    ctx.frameSet = m_frameSet;
    ctx.materialSet = m_materialSet;
    ctx.cascadeShadowMap = m_cascadeShadowMap.get();
    ctx.reflectionProbeManager = m_reflectionProbeManager.get();
    ctx.ensureResourceSets = [this]() { EnsureCurrentFrameResourceSets(); };
    ctx.acquirePassDrawSet = [this]() { return AcquirePassDrawSet(); };
    ctx.acquireMaterialDrawSet = [this]() { return AcquireMaterialDrawSet(); };
    ctx.acquireSceneDrawSet = [this]() { return AcquireSceneDrawSet(); };
    ctx.ensureIbl = [this](const RenderFrameData& fd) { return EnsureIBLFromFrameData(fd); };
    ctx.buildForwardPassSharedState = [this](const RenderFrameData& fd, const CameraView& cv, rhi::IDescriptorSet* passSet,
                                              rhi::IDescriptorSet* sceneSet, bool usePrev) {
        BuildForwardPassSharedState(fd, cv, passSet, sceneSet, usePrev);
    };
    ForwardOpaqueRenderForCamera(cmd, frameData, cam, opaqueDrawables, ctx);
}

void RenderPipeline::RenderForwardTransparent(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam) {
    ForwardTransparentRenderContext ctx{};
    ctx.device = m_device;
    ctx.shaderCache = m_shaderCache;
    ctx.passResourceBuilder = m_passResourceBuilder.get();
    ctx.frameUbo = m_frameUbo;
    ctx.bonesBuffer = m_bonesBuffer;
    ctx.sceneLightingBuffer = m_sceneLightingBuffer;
    ctx.lightIndexBuffer = m_lightIndexBuffer;
    ctx.bindlessHeap = m_bindlessHeap;
    ctx.frameSet = m_frameSet;
    ctx.materialSet = m_materialSet;
    ctx.cascadeShadowMap = m_cascadeShadowMap.get();
    ctx.reflectionProbeManager = m_reflectionProbeManager.get();
    ctx.ensureResourceSets = [this]() { EnsureCurrentFrameResourceSets(); };
    ctx.acquirePassDrawSet = [this]() { return AcquirePassDrawSet(); };
    ctx.acquireMaterialDrawSet = [this]() { return AcquireMaterialDrawSet(); };
    ctx.acquireSceneDrawSet = [this]() { return AcquireSceneDrawSet(); };
    ctx.ensureIbl = [this](const RenderFrameData& fd) { return EnsureIBLFromFrameData(fd); };
    ctx.buildForwardPassSharedState = [this](const RenderFrameData& fd, const CameraView& cv, rhi::IDescriptorSet* passSet,
                                              rhi::IDescriptorSet* sceneSet, bool usePrev) {
        BuildForwardPassSharedState(fd, cv, passSet, sceneSet, usePrev);
    };
    ForwardTransparentRender(cmd, frameData, cam, ctx);
}

void RenderPipeline::RenderForwardOpaqueStereo(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& leftCam, const CameraView& rightCam) {
    ForwardStereoRenderContext ctx{};
    ctx.device = m_device;
    ctx.shaderCache = m_shaderCache;
    ctx.sceneLightingBuffer = m_sceneLightingBuffer;
    ctx.width = m_width;
    ctx.height = m_height;
    ctx.cascadeShadowMap = m_cascadeShadowMap.get();
    ctx.ensureStereoForwardShader = [this]() { EnsureStereoForwardShader(); };
    ctx.getStereoForwardShader = [this]() { return m_stereoForwardShader.get(); };
    ctx.renderForwardOpaqueForCamera = [this](rhi::ICommandList* cmdOpaque, const RenderFrameData& frameDataOpaque, const CameraView& camViewOpaque,
                                              const std::vector<Drawable>& opaqueDrawables) {
        RenderForwardOpaqueForCamera(cmdOpaque, frameDataOpaque, camViewOpaque, opaqueDrawables);
    };
    ForwardOpaqueRenderStereo(cmd, frameData, leftCam, rightCam, ctx);
}

void RenderPipeline::BuildAndExecuteForwardFrameGraphStereo(const IRenderSceneView& view, const CameraView& leftCam, const CameraView& rightCam) {
    if (!m_renderGraphOrchestrator || !m_cascadeShadowMap || !m_shadowPass || !m_hdrRenderTarget) return;

    m_currentView = &view;
    m_currentLeftCamRaw = leftCam;
    m_currentRightCamRaw = rightCam;
    m_currentLeftCam = backend::AdjustCameraForBackend(m_device, m_currentLeftCamRaw);
    m_currentRightCam = backend::AdjustCameraForBackend(m_device, m_currentRightCamRaw);
    PrepareFrameData(view);
    m_currentFrameData = &m_frameData;

    const FrameRenderContext frc = GetFrameRenderContext();
    RenderContext ctx = MakeRenderContext(frc);
    if (ctx.cmd) ctx.cmd->SetRenderTarget(nullptr);
    PreparePersistentShaderReadInputs(frc.cmd, m_frameData);

    CompileFrameGraphIfNeeded(PipelineMode::Forward, true);
    m_frameGraphExecutionContext = frc;
    m_renderGraphOrchestrator->Execute(ctx);
    m_frameGraphExecutionContext = {};
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
    // Intentionally do not force GPU sync here; explicit sync is handled by Finish()/fences when needed.

    BuildAndExecuteForwardFrameGraphStereo(view, leftCam, rightCam);

    const CameraView cam = m_currentLeftCam;
    if (m_prePostCallback) {
        const RenderScene* sceneForCb = view.GetSceneForCallback();
        if (sceneForCb)
            m_prePostCallback(m_hdrRenderTarget, m_width, m_height, *sceneForCb);
    }

    if (eyelib::debug::GetPresets().debugDrawEnabled.load() && m_device) {
        RenderContext ctx{};
        ctx.device = m_device;
        ctx.cmd = cmd;
        cmd->SetViewport(0, 0, m_width, m_height);
        DebugDrawRenderer::Render(ctx, m_currentLeftCam, m_hdrRenderTarget, m_width, m_height);
    }

    if (m_postProcessor) {
        RenderFrameData probeFrameData;
        probeFrameData.skybox = view.GetSkybox();
        IBL* ibl = EnsureIBLFromFrameData(probeFrameData);
        if (ibl && ibl->IsValid())
            m_reflectionProbeManager->SetGlobalProbeFromPrefiltered(ibl->GetPrefilterTextureRHI(), ibl->GetIrradianceTextureRHI());
        rhi::ITexture* probeTex = nullptr;
        const ReflectionProbe* probe = passes::common::ResolveBestReflectionProbe(m_reflectionProbeManager.get(), cam.viewPos);
        if (probe && probe->IsValid()) probeTex = probe->GetCubemapTexture();
        PostProcessor::Settings settings;
        math::Mat4 invView = glm::inverse(cam.view);
        math::Mat4 invProj = glm::inverse(cam.projection);
        auto skybox = view.GetSkybox();
        rhi::ITexture* skyboxTex = skybox && skybox->IsValid() ? skybox->GetTextureRHI() : nullptr;
        float time = core::Time::TotalTime();
        rhi::ITexture* hdrTex = GetHDRColorTextureRHI();
        settings.ssrNearPlane = cam.nearPlane;
        settings.ssrFarPlane = cam.farPlane;
        m_postProcessor->ApplyPostProcessing(hdrTex, *m_gbuffer, settings, cam.view, cam.projection, invView, invProj, cam.viewPos, skyboxTex, time, probeTex);
    }

    m_prevView = cam.view;
    m_prevProjection = cam.projection;
    // Do not End(): buffer stays recording for blit then EndFrame().
#ifndef NDEBUG
    m_rendering = false;
#endif
}

void RenderPipeline::PrepareForwardPlusFrame(rhi::ICommandList* cmd, const RenderFrameData& frameData) {
    ForwardPlusPrepareContext ctx{};
    ctx.device = m_device;
    ctx.width = m_width;
    ctx.height = m_height;
    ctx.hdrRenderTarget = m_hdrRenderTarget;
    ctx.clusteredLightGrid = m_clusteredLightGrid.get();
    ctx.lightCullPassSystem = m_lightCullPassSystem.get();
    ctx.pointLightBuffer = &m_pointLightBuffer;
    ctx.spotLightBuffer = &m_spotLightBuffer;
    ctx.lightIndexBuffer = &m_lightIndexBuffer;
    ctx.sceneLightingBuffer = m_sceneLightingBuffer;
    ctx.cascadeShadowMap = m_cascadeShadowMap.get();
    ForwardPlusPrepareFrame(cmd, frameData, ctx);
}

void RenderPipeline::RenderForwardPlusOpaqueBatched(rhi::ICommandList* cmd, const RenderFrameData& frameData) {
    ForwardPlusOpaqueRenderContext ctx{};
    ctx.device = m_device;
    ctx.shaderCache = m_shaderCache;
    ctx.passResourceBuilder = m_passResourceBuilder.get();
    ctx.frameUbo = m_frameUbo;
    ctx.bonesBuffer = m_bonesBuffer;
    ctx.sceneLightingBuffer = m_sceneLightingBuffer;
    ctx.lightIndexBuffer = m_lightIndexBuffer;
    ctx.instanceMatrixBuffer = m_instanceMatrixBuffer;
    ctx.bindlessHeap = m_bindlessHeap;
    ctx.frameSet = m_frameSet;
    ctx.materialSet = m_materialSet;
    ctx.cascadeShadowMap = m_cascadeShadowMap.get();
    ctx.reflectionProbeManager = m_reflectionProbeManager.get();
    ctx.ensureResourceSets = [this]() { EnsureCurrentFrameResourceSets(); };
    ctx.acquirePassDrawSet = [this]() { return AcquirePassDrawSet(); };
    ctx.acquireMaterialDrawSet = [this]() { return AcquireMaterialDrawSet(); };
    ctx.acquireSceneDrawSet = [this]() { return AcquireSceneDrawSet(); };
    ctx.ensureIbl = [this](const RenderFrameData& fd) { return EnsureIBLFromFrameData(fd); };
    ctx.buildForwardPassSharedState = [this](const RenderFrameData& fd, const CameraView& cv, rhi::IDescriptorSet* passSet,
                                              rhi::IDescriptorSet* sceneSet, bool usePrev) {
        BuildForwardPassSharedState(fd, cv, passSet, sceneSet, usePrev);
    };
    ForwardPlusOpaqueRenderBatched(cmd, frameData, ctx);
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
    // Intentionally do not force GPU sync here; explicit sync is handled by Finish()/fences when needed.

    const CameraView cam = m_currentFrameData ? m_currentFrameData->camera : backend::AdjustCameraForBackend(m_device, view.GetCamera());
    // Debug: bypass all passes and clear HDR to solid color to verify pipeline -> descriptor -> panel path.
    static bool kDebugBypassAllPasses = false;  // set to false to restore normal rendering
    if (kDebugBypassAllPasses) {
        const rhi::ClearValue clearRed = { 1.f, 0.f, 0.f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearRed);
    } else {
    // Single path: all modes go through frame graph (Deferred/Forward/ForwardPlus use BuildAndExecute*FrameGraph).
    switch (m_mode) {
    case PipelineMode::Deferred:
        BuildAndExecuteFrameGraph(view);
        break;
    case PipelineMode::Forward:
        BuildAndExecuteForwardFrameGraph(view);
        break;
    case PipelineMode::ForwardPlus:
        if (m_clusteredLightGrid)
            BuildAndExecuteForwardPlusFrameGraph(view);
        else
            BuildAndExecuteForwardFrameGraph(view);
        break;
    }

    // EntityID pass: same depth as opaque, write R32UI for picking (editor). Run only when entity IDs provided and count matches.
    if (entityIdsForOpaque && m_entityIdRenderTarget) {
        size_t opaqueCount = 0;
        view.ForEachOpaqueDrawable([&opaqueCount](const Drawable&) { ++opaqueCount; });
        if (opaqueCount == entityIdsForOpaque->size())
            RenderEntityIDPass(view, *entityIdsForOpaque);
    }

    if (m_prePostCallback) {
        const RenderScene* sceneForCb = view.GetSceneForCallback();
        if (sceneForCb)
            m_prePostCallback(m_hdrRenderTarget, m_width, m_height, *sceneForCb);
    }

    if (eyelib::debug::GetPresets().debugDrawEnabled.load() && m_device) {
        RenderContext ctx{};
        ctx.device = m_device;
        ctx.cmd = cmd;
        rhi::IRenderTarget* target = m_hdrRenderTarget;
        DebugDrawRenderer::Render(ctx, cam, target, m_width, m_height);
    }

    if (!m_pendingDebugLinesColored.empty() && m_device) {
        if (!m_editorOverlayPassSystem) m_editorOverlayPassSystem = std::make_unique<EditorOverlayPassSystem>();
        DebugLinesOverlayBuildParams build{};
        build.device = m_device;
        build.cmd = cmd;
        build.renderTarget = m_hdrRenderTarget;
        build.view = m_pendingDebugView;
        build.projection = m_pendingDebugProj;
        build.width = m_width;
        build.height = m_height;
        build.hasAnyLines = !m_pendingDebugLinesColored.empty();
        build.drawLines = [this](rhi::ICommandList* cmdArg, rhi::IRenderTarget* renderTarget, const CameraView& camView, uint32_t width, uint32_t height) {
            RenderContext ctx{};
            ctx.device = m_device;
            ctx.cmd = cmdArg;
            DebugDrawRenderer::RenderLinesWithDepth(ctx, camView, renderTarget, width, height, m_pendingDebugLinesColored);
        };
        DebugLinesOverlayExecuteParams execParams = m_editorOverlayPassSystem->BuildDebugLinesParams(build);
        (void)m_editorOverlayPassSystem->ExecuteDebugLines(execParams);
        m_pendingDebugLinesColored.clear();
    }

    // Final post (tone map, bloom, FXAA, etc.): single pass for all pipeline modes, after frame-graph
    // scene work, entity-id, callbacks, and debug overlays. Not duplicated inside the frame graph recipe.
    const FrameRenderContext postFrameCtx = GetFrameRenderContext();
    PostProcessPassParams postParams = BuildPostProcessPassParams(postFrameCtx, &view, cam);
    ExecutePostProcessPass(postParams);

    } // !kDebugBypassAllPasses

    if (cmd) cmd->SetRenderTarget(nullptr);
    rhi::ITexture* outputTex = GetCurrentFrameOutputColorTextureRHI();
    if (cmd && outputTex)
        rhi::BarrierTextureForShaderRead(cmd, outputTex);

    m_prevView = cam.view;
    m_prevProjection = cam.projection;

    // Do not End() here: buffer must stay recording so app can record blit next, then device->EndFrame() ends and submits. One buffer per frame.
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
    // Recording only; device owns submission (EndFrame submits, Present presents).
    m_device->EndFrame();
    m_device->Present();
}

bool RenderPipeline::HasValidOutput() const {
    return m_hdrRenderTarget != nullptr && m_gbuffer != nullptr;
}

void RenderPipeline::EnsureStereoForwardShader() {
    if (m_stereoForwardShader && m_stereoForwardShader->IsValid()) return;
    if (!m_device) return;
    if (m_usingShared && m_usingShared->stereoForwardShader && m_usingShared->stereoForwardShader->IsValid()) {
        m_stereoForwardShader = m_usingShared->stereoForwardShader;
        return;
    }
    m_stereoForwardShader = std::make_shared<Shader>();
    if (!m_stereoForwardShader->Load(m_device, "passes/forward/forward_stereo", Mesh::GetVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList, rhi::PipelineLayoutKind::Forward,
                                     std::optional{rhi::StandardAttachmentFormats::ForwardColorDepth()})) {
        core::Log::WarnF("Stereo forward shader failed to load (passes/forward/forward_stereo). Stereo preview will fall back to dual pass.");
        m_stereoForwardShader.reset();
        return;
    }
    if (m_sharedResources) m_sharedResources->stereoForwardShader = m_stereoForwardShader;
}

void RenderPipeline::EnsureOutlinePostSetFilled(rhi::ITexture* sceneTex, rhi::ITexture* idTex) {
    if (!m_device || !sceneTex || !idTex) return;
    const uint32_t frameIndex = m_device->GetCurrentFrameIndex() % kOutlinePostSetsFrames;
    const uint32_t frameNumber = m_device->GetFrameNumber();
    rhi::IDescriptorSet* outlineSet = m_outlinePostSets[frameIndex];
    if (!outlineSet) return;
    if (m_lastOutlinePostSetFilledFrameNumber == frameNumber &&
        m_cachedOutlineSceneTexture == sceneTex &&
        m_cachedOutlineIdTexture == idTex) {
        return;
    }
    outlineSet->UpdateTexture(0, sceneTex);
    outlineSet->UpdateTexture(1, idTex);
    m_lastOutlinePostSetFilledFrameNumber = frameNumber;
    m_cachedOutlineSceneTexture = sceneTex;
    m_cachedOutlineIdTexture = idTex;
}

void RenderPipeline::AddSkyboxPassToFrameGraph(FGTextureHandle hdrColor, bool useGbufferDepth) {
    FrameGraph* fg = m_renderGraphOrchestrator ? m_renderGraphOrchestrator->GetFrameGraph() : nullptr;
    if (!fg) return;
    fg->AddPass("Skybox",
        [this, useGbufferDepth](const FGPassResources&) {
            if (!m_currentFrameData)
                return;
            if (m_currentFrameData->skybox && m_currentFrameData->skybox->IsValid())
                RenderSkyboxImpl(m_currentFrameData->camera, m_currentFrameData->skybox, useGbufferDepth);
            else
                RenderProceduralSkyboxImpl(m_currentFrameData->camera, useGbufferDepth);
            debug::Profiler::AddFrameCounter("drawCalls", 1);
            debug::Profiler::AddFrameCounter("triangles", 2);
            debug::Profiler::AddFrameCounter("stateChanges", 1);
        },
        {}, { hdrColor }, true);
}

void RenderPipeline::RenderSkyboxImpl(const CameraView& camera, const std::shared_ptr<TextureCube>& skybox, bool useGbufferDepth, rhi::IRenderTarget* targetOverride) {
    if (!m_skyboxPassSystem) return;
    EnsureCurrentFrameResourceSets();
    rhi::IRenderTarget* rt = targetOverride ? targetOverride : m_hdrRenderTarget;
    const uint32_t vpW = rt ? rt->GetWidth() : m_width;
    const uint32_t vpH = rt ? rt->GetHeight() : m_height;
    SkyboxDrawContext ctx{};
    ctx.device = m_device;
    ctx.cmd = GetRHICommandList();
    ctx.camera = camera;
    ctx.renderTarget = rt;
    ctx.hdrRenderTarget = m_hdrRenderTarget;
    ctx.gbufferRenderTarget = m_gbuffer ? m_gbuffer->GetRenderTargetRHI() : nullptr;
    ctx.frameSet = m_frameSet;
    ctx.viewportWidth = vpW;
    ctx.viewportHeight = vpH;
    ctx.useGbufferDepth = useGbufferDepth;
    ctx.isVulkan = backend::IsVulkan(m_device);
    (void)m_skyboxPassSystem->RenderCubemap(ctx, skybox);
}

void RenderPipeline::RenderProceduralSkyboxImpl(const CameraView& camera, bool useGbufferDepth, rhi::IRenderTarget* targetOverride) {
    if (!m_skyboxPassSystem) return;
    EnsureCurrentFrameResourceSets();
    rhi::IRenderTarget* rt = targetOverride ? targetOverride : m_hdrRenderTarget;
    const uint32_t vpW = rt ? rt->GetWidth() : m_width;
    const uint32_t vpH = rt ? rt->GetHeight() : m_height;
    SkyboxDrawContext ctx{};
    ctx.device = m_device;
    ctx.cmd = GetRHICommandList();
    ctx.camera = camera;
    ctx.renderTarget = rt;
    ctx.hdrRenderTarget = m_hdrRenderTarget;
    ctx.gbufferRenderTarget = m_gbuffer ? m_gbuffer->GetRenderTargetRHI() : nullptr;
    ctx.frameSet = m_frameSet;
    ctx.viewportWidth = vpW;
    ctx.viewportHeight = vpH;
    ctx.useGbufferDepth = useGbufferDepth;
    ctx.isVulkan = backend::IsVulkan(m_device);
    (void)m_skyboxPassSystem->RenderProcedural(ctx);
}

void RenderPipeline::RenderSkybox(const IRenderSceneView& view, bool useGbufferDepth, rhi::IRenderTarget* targetOverride) {
    const CameraView cam = backend::AdjustCameraForBackend(m_device, view.GetCamera());
    if (view.GetSkybox() && view.GetSkybox()->IsValid())
        RenderSkyboxImpl(cam, view.GetSkybox(), useGbufferDepth, targetOverride);
    else
        RenderProceduralSkyboxImpl(cam, useGbufferDepth, targetOverride);
}

void RenderPipeline::RenderSceneToTarget(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam, rhi::IRenderTarget* rt) {
    if (!cmd || !rt) return;
    const uint32_t w = rt->GetWidth();
    const uint32_t h = rt->GetHeight();
    rhi::ClearValue clear{};
    clear.color[0] = 0.f; clear.color[1] = 0.f; clear.color[2] = 0.f; clear.color[3] = 1.f;
    rhi::ClearAndSetRenderTargetWithViewport(cmd, rt, 0, 0, w, h, clear);
    cmd->SetDepthTest(true);
    cmd->SetCullFace(false);
    RenderFrameData captureFrame = frameData;
    captureFrame.camera = cam;
    RenderForwardOpaqueForCamera(cmd, captureFrame, captureFrame.camera, captureFrame.visibleOpaque);
    if (frameData.skybox && frameData.skybox->IsValid())
        RenderSkyboxImpl(cam, frameData.skybox, false, rt);
    else
        RenderProceduralSkyboxImpl(cam, false, rt);
    cmd->SetCullFace(true);
    cmd->SetRenderTarget(nullptr);
}

void RenderPipeline::EnsureOutlinePostShader() {
    if (m_outlinePostShader && m_outlinePostShader->IsValid()) return;
    if (!m_device) return;
    if (m_usingShared && m_usingShared->outlinePostShader && m_usingShared->outlinePostShader->IsValid()) {
        m_outlinePostShader = m_usingShared->outlinePostShader;
    } else {
        m_outlinePostShader = std::make_shared<Shader>();
        rhi::DepthStencilState postDepth;
        postDepth.depthTest = false;
        postDepth.depthWrite = false;
        if (!m_outlinePostShader->Load(m_device, "passes/outline_post/outline_post", Mesh::GetFullscreenQuadVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList, &postDepth, rhi::PipelineLayoutKind::OutlinePost,
                                       std::optional{rhi::StandardAttachmentFormats::PostHDRWithDepth()})) {
            core::Log::WarnF("Outline post shader failed to load (passes/outline_post/outline_post). Run from project root or ensure assets/shaders/opengl/ is available.");
            return;
        }
        if (m_sharedResources) m_sharedResources->outlinePostShader = m_outlinePostShader;
    }
    rhi::IPipelineLayout* outlineLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::OutlinePost);
    if (outlineLayout) {
        for (rhi::IDescriptorSet*& set : m_outlinePostSets) {
            if (!set) set = m_device->CreateDescriptorSet(outlineLayout);
        }
    }
}

void RenderPipeline::EnsureEntityIdShader() {
    if (m_entityIdShader && m_entityIdShader->IsValid()) return;
    if (!m_device) return;
    if (m_usingShared && m_usingShared->entityIdShader && m_usingShared->entityIdShader->IsValid()) {
        m_entityIdShader = m_usingShared->entityIdShader;
        return;
    }
    m_entityIdShader = std::make_shared<Shader>();
    rhi::DepthStencilState entityIdDepth{};
    entityIdDepth.depthTest = true;
    entityIdDepth.depthWrite = false;
    entityIdDepth.depthCompare = rhi::DepthCompare::Less;
    if (!m_entityIdShader->Load(m_device, "passes/entityid/entityid", Mesh::GetVertexLayoutRHI(),
                                rhi::PrimitiveTopology::TriangleList, &entityIdDepth, rhi::PipelineLayoutKind::PushOnly,
                                std::optional{rhi::StandardAttachmentFormats::EntityId()})) {
        core::Log::WarnF("EntityID shader failed to load (passes/entityid/entityid). Run from project root or ensure assets/shaders/opengl/ is available.");
        return;
    }
    if (m_sharedResources) m_sharedResources->entityIdShader = m_entityIdShader;
}

void RenderPipeline::EnsureStreamingNV12Pass() {
    if (m_streamingNV12Pipeline || !m_device) return;
    if (!backend::SupportsCompute(m_device)) return;
    std::string path;
    if (!m_device->ResolveShaderPath("passes/streaming/rgba_to_nv12", rhi::ShaderStage::Compute, path)) {
        core::Log::WarnF("Streaming NV12: ResolveShaderPath failed for passes/streaming/rgba_to_nv12");
        return;
    }
    std::string src;
    if (!LoadShaderSource(path, src)) {
        core::Log::WarnF("Streaming NV12: LoadShaderSource failed for %s", path.c_str());
        return;
    }
    m_streamingNV12Shader = m_device->CreateShader(rhi::ShaderStage::Compute, src.c_str(), src.size());
    if (!m_streamingNV12Shader || !m_streamingNV12Shader->IsValid()) {
        core::Log::Warn("Streaming NV12: CreateShader failed");
        if (m_streamingNV12Shader) { m_device->DestroyShader(m_streamingNV12Shader); m_streamingNV12Shader = nullptr; }
        return;
    }
    m_streamingNV12Pipeline = m_device->CreateComputePipeline(m_streamingNV12Shader);
    if (!m_streamingNV12Pipeline) {
        core::Log::Warn("Streaming NV12: CreateComputePipeline failed");
        m_device->DestroyShader(m_streamingNV12Shader);
        m_streamingNV12Shader = nullptr;
        return;
    }
}

void RenderPipeline::RunStreamingNV12Pass(rhi::IRenderTarget* sourceRT, void* nv12BufferHandle, size_t nv12Size, uint32_t width, uint32_t height, uint32_t pitch) {
    EnsureStreamingNV12Pass();
    rhi::ICommandList* cmd = GetRHICommandList();
    if (!m_streamingPassSystem) m_streamingPassSystem = std::make_unique<StreamingPassSystem>();
    StreamingNv12BuildParams build{};
    build.device = m_device;
    build.cmd = cmd;
    build.pipeline = m_streamingNV12Pipeline;
    build.sourceRT = sourceRT;
    build.nv12BufferHandle = nv12BufferHandle;
    build.nv12Size = nv12Size;
    build.width = width;
    build.height = height;
    build.pitch = pitch;
    build.useTypedPush = backend::UseTypedPushConstants(m_device);
    StreamingNv12ExecuteParams execParams = m_streamingPassSystem->BuildNv12Params(build);
    (void)m_streamingPassSystem->ExecuteNv12(execParams);
}

void RenderPipeline::RenderEntityIDPass(const IRenderSceneView& view, const std::vector<uint32_t>& entityIdsForOpaque) {
    if (!m_entityIdRenderTarget || entityIdsForOpaque.empty()) return;
    EnsureEntityIdShader();
    if (!m_entityIdShader || !m_entityIdShader->IsValid()) return;

    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;

    const CameraView cam = m_currentFrameData ? m_currentFrameData->camera : backend::AdjustCameraForBackend(m_device, view.GetCamera());
    const bool useTypedEntityIdPush = backend::UseTypedPushConstants(m_device);
    if (!m_editorOverlayPassSystem) m_editorOverlayPassSystem = std::make_unique<EditorOverlayPassSystem>();
    EntityIdOverlayBuildParams build{};
    build.device = m_device;
    build.cmd = cmd;
    build.entityIdRenderTarget = m_entityIdRenderTarget;
    build.entityIdShader = m_entityIdShader.get();
    build.camView = cam;
    build.width = m_width;
    build.height = m_height;
    build.useTypedPush = useTypedEntityIdPush;
    build.drawItems = [this, &view, &entityIdsForOpaque](rhi::ICommandList* cmdArg, Shader* entityIdShader, bool useTypedPush) {
        size_t index = 0;
        view.ForEachOpaqueDrawable([&](const Drawable& d) {
            if (!d.mesh || index >= entityIdsForOpaque.size()) { ++index; return; }
            uint32_t id = entityIdsForOpaque[index++];
            if (useTypedPush) {
                abi::WriteEntityIdFragPushConstants(cmdArg, id);
                abi::WriteEntityIdModelPushConstants(cmdArg, d.transform);
            } else {
                entityIdShader->SetUint("u_EntityID", id);
                entityIdShader->SetMat4("u_Model", d.transform);
            }
            d.mesh->Bind(m_device);
            { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device, d.submeshIndex); }
            d.mesh->Unbind();
        });
    };
    EntityIdOverlayExecuteParams execParams = m_editorOverlayPassSystem->BuildEntityIdParams(build);
    (void)m_editorOverlayPassSystem->ExecuteEntityId(execParams);
}

void RenderPipeline::RenderOutlineFromIdBuffer(uint32_t selectedPackedId, float r, float g, float b) {
    if (selectedPackedId == 0 || !m_hdrRenderTarget || !m_entityIdRenderTarget || !m_outlineCompositeTemp || !m_device) return;
    EnsureOutlinePostShader();
    if (!m_outlinePostShader || !m_outlinePostShader->IsValid()) return;
    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;
    if (!m_editorOverlayPassSystem) m_editorOverlayPassSystem = std::make_unique<EditorOverlayPassSystem>();
    rhi::IDescriptorSet* outlineSet = m_outlinePostSets[m_device->GetCurrentFrameIndex() % kOutlinePostSetsFrames];
    OutlineOverlayBuildParams build{};
    build.device = m_device;
    build.cmd = cmd;
    build.hdrRenderTarget = m_hdrRenderTarget;
    build.entityIdRenderTarget = m_entityIdRenderTarget;
    build.outlineCompositeTemp = m_outlineCompositeTemp;
    build.outlineShader = m_outlinePostShader.get();
    build.outlineSet = outlineSet;
    build.width = m_width;
    build.height = m_height;
    build.selectedPackedId = selectedPackedId;
    build.colorR = r;
    build.colorG = g;
    build.colorB = b;
    build.useTypedOutlinePush = backend::UseTypedPushConstants(m_device);
    build.bindDescriptorSet = [this](rhi::ICommandList* cmdArg, rhi::IRenderTarget* outlineCompositeTemp, rhi::IRenderTarget* entityIdRenderTarget, rhi::IDescriptorSet* outlineSetArg) {
        rhi::ITexture* sceneTex = outlineCompositeTemp ? outlineCompositeTemp->GetColorTexture(0) : nullptr;
        rhi::ITexture* idTex = entityIdRenderTarget ? entityIdRenderTarget->GetColorTexture(0) : nullptr;
        if (sceneTex && idTex && outlineSetArg) {
            EnsureOutlinePostSetFilled(sceneTex, idTex);
            cmdArg->BindDescriptorSets(0, 1, &outlineSetArg);
        }
    };
    build.bindFallbackTextures = [](rhi::ICommandList* cmdArg, rhi::IRenderTarget* outlineCompositeTemp, rhi::IRenderTarget* entityIdRenderTarget) {
        rhi::ITexture* sceneTex = outlineCompositeTemp ? outlineCompositeTemp->GetColorTexture(0) : nullptr;
        rhi::ITexture* idTex = entityIdRenderTarget ? entityIdRenderTarget->GetColorTexture(0) : nullptr;
        if (sceneTex) cmdArg->BindTexture(sceneTex, 0);
        if (idTex) cmdArg->BindTexture(idTex, 1);
    };
    build.writePush = [](rhi::ICommandList* cmdArg, Shader* outlineShader, bool useTypedOutlinePush, uint32_t selectedPackedId, float colorR, float colorG, float colorB, float invW, float invH) {
        if (useTypedOutlinePush) {
            abi::WriteOutlinePostPushConstants(cmdArg, selectedPackedId, math::Vec3(colorR, colorG, colorB), math::Vec2(invW, invH));
        } else {
            outlineShader->SetUint("u_SelectedID", selectedPackedId);
            outlineShader->SetVec3("u_OutlineColor", math::Vec3(colorR, colorG, colorB));
            outlineShader->SetVec2("u_InvSize", math::Vec2(invW, invH));
        }
    };
    OutlineOverlayExecuteParams execParams = m_editorOverlayPassSystem->BuildOutlineParams(build);
    (void)m_editorOverlayPassSystem->ExecuteOutline(execParams);
}

void RenderPipeline::RenderDebugLines(const math::Mat4& view, const math::Mat4& proj,
    const std::vector<std::pair<math::Vec3, math::Vec3>>& lines, float r, float g, float b) {
    if (!m_editorOverlayPassSystem) m_editorOverlayPassSystem = std::make_unique<EditorOverlayPassSystem>();
    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;
    DebugLinesOverlayBuildParams build{};
    build.device = m_device;
    build.cmd = cmd;
    build.renderTarget = m_hdrRenderTarget;
    build.view = view;
    build.projection = proj;
    build.width = m_width;
    build.height = m_height;
    build.hasAnyLines = !lines.empty();
    build.drawLines = [this, &lines, r, g, b](rhi::ICommandList* cmdArg, rhi::IRenderTarget* renderTarget, const CameraView& camView, uint32_t width, uint32_t height) {
        RenderContext ctx{};
        ctx.device = m_device;
        ctx.cmd = cmdArg;
        DebugDrawRenderer::RenderLinesWithDepth(ctx, camView, renderTarget, width, height, lines, math::Vec4(r, g, b, 1.f));
    };
    DebugLinesOverlayExecuteParams execParams = m_editorOverlayPassSystem->BuildDebugLinesParams(build);
    (void)m_editorOverlayPassSystem->ExecuteDebugLines(execParams);
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
    if (m_shaderCache) m_shaderCache->reloadAll();
    if (m_skyboxPassSystem) m_skyboxPassSystem->ReloadShaders();
    if (m_outlinePostShader) m_outlinePostShader->Reload();
    if (m_entityIdShader) m_entityIdShader->Reload();
    if (m_stereoForwardShader) m_stereoForwardShader->Reload();
    if (m_gbufferPassSystem) m_gbufferPassSystem->ReloadShaders();
    if (m_shadowPass) m_shadowPass->ReloadShaders();
    if (m_lightingPass) m_lightingPass->ReloadShaders();
    if (m_postProcessor) m_postProcessor->ReloadShaders();
}

GBuffer* RenderPipeline::GetGBuffer() { return m_gbuffer.get(); }
CascadeShadowMap* RenderPipeline::GetCascadeShadowMap() { return m_cascadeShadowMap.get(); }
PostProcessor* RenderPipeline::GetPostProcessor() { return m_postProcessor.get(); }

const std::vector<FrameGraph::PassInfo>& RenderPipeline::GetFrameGraphPasses() const {
    static const std::vector<FrameGraph::PassInfo> empty;
    const FrameGraph* fg = m_renderGraphOrchestrator ? m_renderGraphOrchestrator->GetFrameGraph() : nullptr;
    return fg ? fg->GetPasses() : empty;
}

} // namespace render
} // namespace eyelib
