#include "eyelib/render/renderer/RenderPipeline.h"
#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/GpuProfilerRHI.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/lighting/ClusteredLightGrid.h"
#include "eyelib/render/post/PostProcessor.h"
#include "eyelib/render/ibl/IBL.h"
#include "eyelib/render/reflection/ReflectionProbeManager.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/passes/GBufferPass.h"
#include "eyelib/render/shadows/ShadowPass.h"
#include "eyelib/render/passes/LightingPass.h"
#include "eyelib/render/passes/TransparencyPass.h"
#include "eyelib/render/passes/GpuCullPass.h"
#include "eyelib/render/lighting/LightCullPass.h"
#include "eyelib/render/lighting/SceneLighting.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/render/DebugDrawRenderer.h"
#include "eyelib/render/RenderContext.h"
#include "eyelib/render/2d/Primitives.h"
#include "eyelib/debug/Presets.h"
#include "eyelib/debug/Profiler.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/framegraph/FrameGraphRecipe.h"
#include "eyelib/render/submission/DrawBatch.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialInstance.h"
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
#include "eyelib/render/CameraConventions.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/RenderBackend.h"

#include "eyelib/math/TransformUtils.h"
#include "eyelib/math/Frustum.h"
#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstdint>
#include <glm/gtc/matrix_inverse.hpp>
#include <span>
#include <cstring>
#include <string>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;
constexpr uint32_t kDefaultDirectionalShadowResolution = 1536u;
constexpr uint32_t kDefaultDirectionalShadowCascades = 3u;
constexpr float kMaxDirectionalShadowDistance = 180.0f;

bool IsForwardDrawableVisible(const Drawable& d, const math::Frustum& frustum) {
    if (!d.mesh) return false;
    // Keep skinned meshes conservative to avoid incorrect culling with animated bounds.
    if (d.bones && d.boneCount > 0) return true;
    const math::AABB worldAabb = d.mesh->GetAABB().Transform(d.transform);
    return frustum.Contains(worldAabb) != 0;
}

float DistanceToCameraTransparent(const math::Mat4& transform, const math::Vec3& viewPos) {
    math::Vec3 p(transform[3][0], transform[3][1], transform[3][2]);
    math::Vec3 d(p.x - viewPos.x, p.y - viewPos.y, p.z - viewPos.z);
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

void ApplyMaterialBlendCullTransparent(rhi::ICommandList* cmd, const Material* material) {
    if (!cmd || !material) return;
    cmd->SetCullFace(!material->GetTwoSided());
    switch (material->GetBlending()) {
    case Blending::Opaque:
        cmd->SetBlend(false);
        cmd->SetDepthWrite(true);
        break;
    case Blending::Translucent:
        cmd->SetBlend(true);
        cmd->SetBlendFunc(rhi::BlendFactor::SrcAlpha, rhi::BlendFactor::OneMinusSrcAlpha);
        cmd->SetDepthWrite(false);
        break;
    case Blending::Additive:
        cmd->SetBlend(true);
        cmd->SetBlendFunc(rhi::BlendFactor::One, rhi::BlendFactor::One);
        cmd->SetDepthWrite(false);
        break;
    case Blending::Modulate:
        cmd->SetBlend(true);
        cmd->SetBlendFunc(rhi::BlendFactor::DstColor, rhi::BlendFactor::Zero);
        cmd->SetDepthWrite(false);
        break;
    }
}

/** Forward/Forward+ PBR uses bindless heap + five resource-space sets + frame UBO only (no legacy bind path). */
bool ForwardPbrResourceBindingsReady(rhi::IDevice* device, rhi::ICommandList* cmd, rhi::IDescriptorSet* frameSet,
    rhi::IDescriptorSet* passSet, rhi::IDescriptorSet* materialSet, rhi::IDescriptorSet* sceneBuffersSet,
    rhi::IBuffer* frameUbo, rhi::IBindlessHeap* bindlessHeap) {
    if (!device || !cmd || !frameSet || !passSet || !materialSet || !sceneBuffersSet || !frameUbo || !bindlessHeap)
        return false;
    return bindlessHeap->GetDescriptorSet() != nullptr;
}

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
    m_frameGraph.reset();
    m_lightingPass.reset();
    if (m_shadowPass && m_device) m_shadowPass->Shutdown();
    m_shadowPass.reset();
    m_gpuDrivenQuad.reset();
    if (m_lightCullPass && m_device) m_lightCullPass->Shutdown();
    m_lightCullPass.reset();
    if (m_device) {
        for (rhi::IDescriptorSet*& s : m_forwardFrameSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
        for (rhi::IDescriptorSet*& s : m_forwardPassSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
        for (rhi::IDescriptorSet*& s : m_forwardTransparentPassSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
        for (rhi::IDescriptorSet*& s : m_forwardMaterialSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
        for (rhi::IDescriptorSet*& s : m_forwardSceneBuffersSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
        for (rhi::IDescriptorSet*& s : m_forwardTransparentSceneBuffersSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
        for (rhi::IDescriptorSet*& s : m_deferredLightingPassSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
        for (auto& drawSetsPerFrame : m_forwardMaterialDrawSets) {
            for (rhi::IDescriptorSet*& s : drawSetsPerFrame) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
            drawSetsPerFrame.clear();
        }
        for (auto& drawSetsPerFrame : m_forwardPassDrawSets) {
            for (rhi::IDescriptorSet*& s : drawSetsPerFrame) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
            drawSetsPerFrame.clear();
        }
        for (auto& drawSetsPerFrame : m_forwardSceneBuffersDrawSets) {
            for (rhi::IDescriptorSet*& s : drawSetsPerFrame) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
            drawSetsPerFrame.clear();
        }
        for (auto& drawSetsPerFrame : m_deferredLightingDrawSets) {
            for (rhi::IDescriptorSet*& s : drawSetsPerFrame) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
            drawSetsPerFrame.clear();
        }
        m_forwardMaterialDrawSetCursor = {};
        m_forwardPassDrawSetCursor = {};
        m_forwardSceneBuffersDrawSetCursor = {};
        m_deferredLightingDrawSetCursor = {};
        m_forwardFrameSet = nullptr;
        m_forwardPassSet = nullptr;
        m_forwardTransparentPassSet = nullptr;
        m_forwardMaterialSet = nullptr;
        m_forwardSceneBuffersSet = nullptr;
        m_forwardTransparentSceneBuffersSet = nullptr;
        m_deferredLightingPassSet = nullptr;
        for (rhi::IDescriptorSet*& set : m_skyboxPostSets) {
            if (set) { m_device->EnqueueDestroyDescriptorSet(set); set = nullptr; }
        }
        m_cachedSkyboxTexture = nullptr;
        if (m_skyboxDummyTexture) { m_device->DestroyTexture(m_skyboxDummyTexture); m_skyboxDummyTexture = nullptr; }
        if (m_forwardFallbackAlbedoTexture) { m_device->DestroyTexture(m_forwardFallbackAlbedoTexture); m_forwardFallbackAlbedoTexture = nullptr; }
        if (m_forwardFallbackNormalTexture) { m_device->DestroyTexture(m_forwardFallbackNormalTexture); m_forwardFallbackNormalTexture = nullptr; }
        if (m_forwardFallbackMetalRoughTexture) { m_device->DestroyTexture(m_forwardFallbackMetalRoughTexture); m_forwardFallbackMetalRoughTexture = nullptr; }
        for (rhi::IDescriptorSet*& s : m_outlinePostSets) {
            if (s) { m_device->EnqueueDestroyDescriptorSet(s); s = nullptr; }
        }
        for (rhi::IBuffer*& b : m_forwardFrameUBOs) { if (b) { m_device->DestroyBuffer(b); b = nullptr; } }
        for (rhi::IBuffer*& b : m_forwardBonesBuffers) { if (b) { m_device->DestroyBuffer(b); b = nullptr; } }
        m_forwardFrameUBO = nullptr;
        m_forwardBonesBuffer = nullptr;
    }
    m_gpuCullPass.reset();
    m_gbufferPass.reset();
    m_shaderCache = nullptr;
    m_usingShared = nullptr;
    m_sharedResources.reset();
    m_cascadeShadowMap.reset();
    m_gbuffer.reset();
    if (m_device) {
        if (m_bindlessHeap) { m_device->DestroyBindlessHeap(m_bindlessHeap); m_bindlessHeap = nullptr; }
        m_bindlessHandleCache.clear();
        if (m_gpuCullTransformBuffer) { m_device->DestroyBuffer(m_gpuCullTransformBuffer); m_gpuCullTransformBuffer = nullptr; }
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

    m_gbufferPass = std::make_unique<GBufferPass>();
    m_gbufferPass->SetShaderCache(m_shaderCache);
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

    m_frameGraph = std::make_unique<FrameGraph>();
    m_frameGraph->SetFrameSize(width, height);
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
    m_frameGraphNeedsCompile = true;
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

void RenderPipeline::UpdateUploads() {
    // Dedicated upload phase: CPU asset updates → staging (UploadHeap) → vkCmdCopyBuffer / vkCmdCopyBufferToImage.
    // All uploads must happen here so they are deterministic and before CompileFrameGraph/Execute. Currently a no-op;
    // move texture/mesh staging uploads from creation paths into this phase as they are refactored.
    (void)m_device;
}

void RenderPipeline::WarmupPipelines() {
    if (!m_device || !m_shaderCache) return;
    EnsureSkyboxShader();
    EnsureProceduralSkyboxShader();
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

namespace {
uint32_t GetIndexCountForDraw(const Mesh* mesh, int32_t submeshIndex) {
    if (!mesh || !mesh->IsValid()) return 0;
    if (submeshIndex >= 0 && static_cast<size_t>(submeshIndex) < mesh->GetSubmeshCount())
        return static_cast<uint32_t>(mesh->GetSubmesh(static_cast<size_t>(submeshIndex)).indexCount);
    return static_cast<uint32_t>(mesh->GetIndexCount());
}

CameraView AdjustCameraForBackend(rhi::IDevice* device, const CameraView& inCamera) {
    const auto backend = device ? device->GetRenderBackend() : ::eyelib::render::RenderBackend::Default;
    return camera::AdjustCameraForBackend(inCamera, backend);
}
} // namespace

/** Enable CPU frustum culling once CPU uses raw (unadjusted) camera clip-space math. */
constexpr bool kEnableCpuFrustumCulling = true;

void RenderPipeline::PrepareFrameData(const IRenderSceneView& view) {
    m_frameData.cameraRaw = view.GetCamera();
    m_frameData.camera = AdjustCameraForBackend(m_device, m_frameData.cameraRaw);
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

uint32_t RenderPipeline::PrepareGpuCullFromFrameData(const RenderFrameData& frameData, const CameraView& cam) {
    constexpr uint32_t kMaxGpuCullDraws = 256;
    m_scratchGpuCullDrawList.clear();
    m_scratchGpuCullTransforms.clear();
    m_gpuCullSingleMesh = true;
    m_gpuCullRepresentativeMesh.reset();
    Mesh* firstMeshPtr = nullptr;
    for (const Drawable& d : frameData.visibleOpaque) {
        if (!d.mesh || !d.mesh->IsValid() || m_scratchGpuCullDrawList.size() >= kMaxGpuCullDraws) continue;
        if (!firstMeshPtr) {
            firstMeshPtr = d.mesh.get();
            m_gpuCullRepresentativeMesh = d.mesh;
        } else if (d.mesh.get() != firstMeshPtr) {
            m_gpuCullSingleMesh = false;
        }
        math::AABB localAABB = d.mesh->GetAABB();
        if (!localAABB.IsValid()) continue;
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
    }
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
    const bool useTypedForwardPush = (m_device && m_device->GetRenderBackend() == ::eyelib::render::RenderBackend::Vulkan);
    if (iblPtr && iblPtr->IsValid()) {
        if (rhi::ITexture* ir = iblPtr->GetIrradianceTextureRHI()) rhi::BarrierTextureForShaderRead(cmd, ir);
        if (rhi::ITexture* pf = iblPtr->GetPrefilterTextureRHI()) rhi::BarrierTextureForShaderRead(cmd, pf);
        if (rhi::ITexture* brdf = iblPtr->GetBRDFLUTTextureRHI()) rhi::BarrierTextureForShaderRead(cmd, brdf);
    }
    const ReflectionProbe* envProbe = m_reflectionProbeManager ? m_reflectionProbeManager->FindClosestProbe(frameData.camera.viewPos) : nullptr;
    if (!envProbe && m_reflectionProbeManager) envProbe = m_reflectionProbeManager->GetGlobalProbe();
    if (envProbe && envProbe->IsValid()) {
        if (rhi::ITexture* ir = envProbe->GetIrradianceMap()) rhi::BarrierTextureForShaderRead(cmd, ir);
        if (rhi::ITexture* pf = envProbe->GetPrefilteredMap()) rhi::BarrierTextureForShaderRead(cmd, pf);
    }
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

void RenderPipeline::RenderOpaqueGeometryToGBuffer(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam, uint32_t gpuCullDrawListCount) {
    if (!m_gbufferPass || !m_gbuffer) return;
    RenderContext ctx = RenderContext::FromDevice(m_device);
    if (!ctx) return;
    EnsureForwardResourceSets();
    if (m_forwardFrameUBO) {
        math::Mat4 viewProj = cam.projection * cam.view;
        float frameUbo[5 * 16];
        std::memcpy(frameUbo, &cam.view[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 16, &cam.projection[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 32, &viewProj[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 48, &m_prevView[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 64, &m_prevProjection[0][0], 16 * sizeof(float));
        m_forwardFrameUBO->Update(frameUbo, 0, 320);
    }
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
        debug::Profiler::AddFrameCounter("drawCalls", 1);
        debug::Profiler::AddFrameCounter("triangles", 2);
        debug::Profiler::AddFrameCounter("stateChanges", 1);
    } else if (useIndirectScene) {
        m_gbufferPass->RenderIndirectWithPerDrawData(*m_gbuffer, ctx,
            m_gpuCullPass->GetIndirectBuffer(),
            m_gpuCullPass->GetDrawCount(),
            m_gpuCullPass->GetIndirectStride(),
            m_gpuCullRepresentativeMesh.get(),
            m_gpuCullTransformBuffer,
            gpuCullDrawListCount,
            cam.view, cam.projection, &m_prevView, &m_prevProjection);
        debug::Profiler::AddFrameCounter("drawCalls", gpuCullDrawListCount);
        const uint32_t idxCount = GetIndexCountForDraw(m_gpuCullRepresentativeMesh.get(), -1);
        debug::Profiler::AddFrameCounter("triangles", (idxCount / 3u) * gpuCullDrawListCount);
        debug::Profiler::AddFrameCounter("stateChanges", 1);
    } else {
        // Phase D: batch by (mesh, material); instanced draw for multi-instance batches, single draws for the rest.
        std::vector<DrawBatch> batches = BuildBatches(frameData.visibleOpaque);
        std::vector<Drawable> singles;
        for (DrawBatch& batch : batches) {
            if (batch.IsInstanced() && m_instanceMatrixBuffer && batch.transforms.size() <= kMaxInstanceBatchSize) {
                const size_t byteSize = batch.transforms.size() * sizeof(math::Mat4);
                m_instanceMatrixBuffer->Update(batch.transforms.data(), 0, static_cast<uint32_t>(byteSize));
                m_gbufferPass->RenderInstanced(*m_gbuffer, m_device, cmd,
                    batch.mesh.get(), batch.material.get(), batch.materialInstance.get(), m_instanceMatrixBuffer,
                    static_cast<uint32_t>(batch.transforms.size()),
                    cam.view, cam.projection, m_prevView, m_prevProjection, batch.submeshIndex);
                debug::Profiler::AddFrameCounter("drawCalls", 1);
                debug::Profiler::AddFrameCounter("triangles", (GetIndexCountForDraw(batch.mesh.get(), batch.submeshIndex) / 3u) * static_cast<uint32_t>(batch.transforms.size()));
                debug::Profiler::AddFrameCounter("stateChanges", 1);
            } else {
                for (size_t i = 0; i < batch.transforms.size(); ++i)
                    singles.push_back(batch.ToDrawable(i));
            }
        }
        if (!singles.empty()) {
            for (const Drawable& d : singles) {
                debug::Profiler::AddFrameCounter("drawCalls", 1);
                debug::Profiler::AddFrameCounter("triangles", GetIndexCountForDraw(d.mesh.get(), d.submeshIndex) / 3u);
                debug::Profiler::AddFrameCounter("stateChanges", 1);
            }
            m_gbufferPass->Render(*m_gbuffer, ctx, singles, cam.view, cam.projection, m_prevView, m_prevProjection);
        }
    }
}

void RenderPipeline::RunShadowPassBatched(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& camView) {
    if (!m_shadowPass || !frameData.light.castShadows) return;
    const float aspect = (camView.projection[0][0] != 0.0f)
        ? std::abs(camView.projection[1][1] / camView.projection[0][0])
        : 1.0f;
    const float shadowFar = std::min(camView.farPlane, kMaxDirectionalShadowDistance);
    std::vector<DrawBatch> batches = BuildBatches(frameData.allOpaque);
    std::vector<Drawable> singles;
    for (DrawBatch& batch : batches) {
        if (batch.IsInstanced() && m_instanceMatrixBuffer && batch.transforms.size() <= kMaxInstanceBatchSize) {
            const size_t byteSize = batch.transforms.size() * sizeof(math::Mat4);
            m_instanceMatrixBuffer->Update(batch.transforms.data(), 0, static_cast<uint32_t>(byteSize));
            m_shadowPass->RenderInstanced(*m_cascadeShadowMap, m_device, cmd,
                batch.mesh.get(), batch.material.get(), m_instanceMatrixBuffer,
                static_cast<uint32_t>(batch.transforms.size()),
                frameData.light.direction, camView.view, camView.projection, camView.viewPos,
                camView.nearPlane, shadowFar, aspect, batch.submeshIndex);
            debug::Profiler::AddFrameCounter("drawCalls", 1);
            debug::Profiler::AddFrameCounter("triangles", (GetIndexCountForDraw(batch.mesh.get(), batch.submeshIndex) / 3u) * static_cast<uint32_t>(batch.transforms.size()));
            debug::Profiler::AddFrameCounter("stateChanges", 1);
        } else {
            for (size_t i = 0; i < batch.transforms.size(); ++i)
                singles.push_back(batch.ToDrawable(i));
        }
    }
    if (!singles.empty()) {
        for (const Drawable& d : singles) {
            debug::Profiler::AddFrameCounter("drawCalls", 1);
            debug::Profiler::AddFrameCounter("triangles", GetIndexCountForDraw(d.mesh.get(), d.submeshIndex) / 3u);
            debug::Profiler::AddFrameCounter("stateChanges", 1);
        }
        RenderContext c = RenderContext::FromDevice(m_device);
        m_shadowPass->Render(*m_cascadeShadowMap, c.device, c.cmd,
            [&singles](std::function<void(const Drawable&)> cb) { for (const auto& d : singles) cb(d); },
            frameData.light.direction, camView.view, camView.projection, camView.viewPos, camView.nearPlane, shadowFar, aspect);
    }
}

RenderPipeline::ShadowPassParams RenderPipeline::BuildShadowPassParams(const RenderFrameData* frameData, const CameraView& camView) {
    ShadowPassParams p{};
    p.frameData = frameData;
    p.camView = camView;
    return p;
}

RenderPipeline::DeferredLightingPassParams RenderPipeline::BuildDeferredLightingPassParams(const RenderFrameData* frameData, const CameraView& camView,
                                                                                           const rhi::ClearValue& clearColor) {
    DeferredLightingPassParams p{};
    p.frameData = frameData;
    p.camView = camView;
    p.clearColor = clearColor;
    if (!frameData || !m_lightingPass || !m_gbuffer)
        return p;

    EnsureForwardResourceSets();
    p.lightParams = frameData->light;
    p.ibl = EnsureIBLFromFrameData(*frameData);
    p.cascadeForLighting = p.lightParams.castShadows ? m_cascadeShadowMap.get() : nullptr;

    math::Mat4 lightSpaceMatrix(1.0f);
    if (p.cascadeForLighting && p.cascadeForLighting->GetSplitCount() > 0)
        lightSpaceMatrix = p.cascadeForLighting->GetLightSpaceMatrix(0);

    FillSceneLightingBuffer(m_sceneLightingBuffer, p.lightParams, camView.viewPos, lightSpaceMatrix, 0.005f,
                            p.lightParams.castShadows, &frameData->pointLights, &frameData->spotLights, p.cascadeForLighting);
    if (m_forwardFrameUBO) {
        math::Mat4 viewProj = camView.projection * camView.view;
        float frameUbo[5 * 16];
        std::memcpy(frameUbo, &camView.view[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 16, &camView.projection[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 32, &viewProj[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 48, &m_prevView[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 64, &m_prevProjection[0][0], 16 * sizeof(float));
        m_forwardFrameUBO->Update(frameUbo, 0, 320);
    }

    p.lightingSetForCall = AcquireDeferredLightingDrawSet();
    if (p.lightingSetForCall) {
        p.lightingSetForCall->UpdateTexture(0, m_gbuffer->GetPositionTextureRHI());
        p.lightingSetForCall->UpdateTexture(1, m_gbuffer->GetNormalTextureRHI());
        p.lightingSetForCall->UpdateTexture(2, m_gbuffer->GetAlbedoTextureRHI());
        p.lightingSetForCall->UpdateTexture(3, m_gbuffer->GetPBRParamsTextureRHI());
        if (p.cascadeForLighting)
            p.lightingSetForCall->UpdateTexture(4, p.cascadeForLighting->GetDepthTextureRHI());
        if (m_sceneLightingBuffer)
            p.lightingSetForCall->UpdateStorageBuffer(5, m_sceneLightingBuffer, 0, 0);
        if (p.cascadeForLighting) {
            rhi::IBuffer* lightSpaceBuf = p.cascadeForLighting->GetLightSpaceBufferRHI();
            if (lightSpaceBuf)
                p.lightingSetForCall->UpdateUniformBuffer(6, lightSpaceBuf, 0, 0);
        }
        if (p.ibl && p.ibl->IsValid()) {
            if (rhi::ITexture* ir = p.ibl->GetIrradianceTextureRHI()) p.lightingSetForCall->UpdateTexture(7, ir);
            if (rhi::ITexture* pf = p.ibl->GetPrefilterTextureRHI()) p.lightingSetForCall->UpdateTexture(8, pf);
            if (rhi::ITexture* brdf = p.ibl->GetBRDFLUTTextureRHI()) p.lightingSetForCall->UpdateTexture(9, brdf);
        }
        m_lightingPass->SetResourceSpaceSets(m_forwardFrameSet, p.lightingSetForCall);
    }
    return p;
}

RenderPipeline::TransparencyPassParams RenderPipeline::BuildTransparencyPassParams(const RenderFrameData* frameData, const CameraView& camView) {
    TransparencyPassParams p{};
    p.frameData = frameData;
    p.camView = camView;
    return p;
}

RenderPipeline::PostProcessPassParams RenderPipeline::BuildPostProcessPassParams(const IRenderSceneView* sceneView, const CameraView& camView) {
    PostProcessPassParams p{};
    p.sceneView = sceneView;
    p.camView = camView;
    p.ibl = EnsureIBLFromFrameData(m_frameData);
    if (p.ibl && p.ibl->IsValid() && m_reflectionProbeManager)
        m_reflectionProbeManager->SetGlobalProbeFromPrefiltered(p.ibl->GetPrefilterTextureRHI(), p.ibl->GetIrradianceTextureRHI());
    if (m_reflectionProbeManager) {
        const ReflectionProbe* probe = m_reflectionProbeManager->FindClosestProbe(camView.viewPos);
        if (!probe) probe = m_reflectionProbeManager->GetGlobalProbe();
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

void RenderPipeline::ExecuteShadowPass(rhi::ICommandList* cmd, const ShadowPassParams& params) {
    if (!params.frameData) return;
    RunShadowPassBatched(cmd, *params.frameData, params.camView);
}

void RenderPipeline::ExecuteDeferredLightingPass(rhi::ICommandList* cmd, const DeferredLightingPassParams& params) {
    if (!params.frameData || !m_lightingPass || !m_gbuffer) return;
    const CameraView& camView = params.camView;
    const LightParams& lightParams = params.lightParams;
    if (cmd && m_hdrRenderTarget)
        rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, params.clearColor);
    RenderContext c = RenderContext::FromDevice(m_device);
    m_lightingPass->Render(*m_gbuffer, c, lightParams, camView.view, camView.projection, camView.viewPos, params.cascadeForLighting,
                           &params.frameData->pointLights, &params.frameData->spotLights, m_sceneLightingBuffer, params.ibl);
    debug::Profiler::AddFrameCounter("drawCalls", 1);
    debug::Profiler::AddFrameCounter("triangles", 2);
    debug::Profiler::AddFrameCounter("stateChanges", 1);
    if (cmd) cmd->SetRenderTarget(nullptr);
}

void RenderPipeline::ExecuteTransparencyPass(rhi::ICommandList* cmd, const TransparencyPassParams& params) {
    if (!params.frameData) return;
    if (m_device && m_hdrRenderTarget)
        m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_hdrRenderTarget);
    if (cmd && m_hdrRenderTarget) {
        cmd->SetRenderTarget(m_hdrRenderTarget);
        cmd->SetViewport(0, 0, m_width, m_height);
    }
    if (m_device && m_device->GetRenderBackend() == RenderBackend::Vulkan && m_shaderCache)
        RenderForwardTransparent(cmd, *params.frameData, params.camView);
    else {
        RenderContext c = RenderContext::FromDevice(m_device);
        TransparencyPass::Render(c, params.frameData->visibleTransparent, params.camView.view, params.camView.projection, params.camView.viewPos, m_shaderCache);
    }
    if (cmd) cmd->SetRenderTarget(nullptr);
}

void RenderPipeline::ExecutePostProcessPass(rhi::ICommandList* cmd, const PostProcessPassParams& params) {
    if (!m_postProcessor || !m_gbuffer) return;
    PostProcessor::Settings settings{};
    settings.ssrNearPlane = params.ssrNearPlane;
    settings.ssrFarPlane = params.ssrFarPlane;
    (void)cmd;
    (void)m_postProcessor->ApplyPostProcessing(
        params.hdrTex, *m_gbuffer, settings,
        params.camView.view, params.camView.projection,
        params.invView, params.invProj, params.camView.viewPos, params.skyboxTex, params.time, params.probeTex);
}

void RenderPipeline::CompileFrameGraphIfNeeded(PipelineMode mode, bool stereo) {
    if (!m_frameGraph) return;
    const bool needsCompile = m_frameGraphNeedsCompile
        || m_frameGraphLastWidth != m_width || m_frameGraphLastHeight != m_height
        || m_frameGraphLastMode != mode || m_frameGraphLastStereo != stereo;
    if (!needsCompile) return;

    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);

    if (mode == PipelineMode::Deferred)
        BuildDeferredGraph();
    else if (mode == PipelineMode::ForwardPlus)
        BuildForwardPlusGraph();
    else if (stereo)
        BuildForwardStereoGraph();
    else
        BuildForwardGraph();

    m_frameGraph->Compile(m_device);
    m_frameGraphLastWidth = m_width;
    m_frameGraphLastHeight = m_height;
    m_frameGraphLastMode = mode;
    m_frameGraphLastStereo = stereo;
    m_frameGraphNeedsCompile = false;
}

void RenderPipeline::BuildDeferredGraph() {
    if (!m_frameGraph || !m_gbuffer || !m_cascadeShadowMap) return;
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
    nameToHandle["Shadow"] = m_frameGraph->ImportTextureRHI("Shadow", m_cascadeShadowMap->GetDepthTextureRHI(), m_cascadeShadowMap->GetResolution(), m_cascadeShadowMap->GetResolution());

    FrameGraphRecipe recipe;
    recipe.push_back({ "GPU Cull", {}, {}, true });
    recipe.push_back({ "Shadow", {}, { "Shadow" }, true });
    recipe.push_back({ "GBuffer", {}, { "GBufferPos", "GBufferNorm", "GBufferAlb", "GBufferPBR", "GBufferVelocity", "GBufferDepth" }, true });
    recipe.push_back({ "Copy depth for occlusion", { "GBufferDepth" }, {}, true });
    recipe.push_back({ "Lighting", { "GBufferPos", "GBufferNorm", "GBufferAlb", "GBufferPBR", "Shadow" }, { "HDR" }, true });
    recipe.push_back({ "Transparency", {}, { "HDR" }, true });

    auto getExecutor = [this](const std::string& name) -> FrameGraph::ExecuteFunc {
        if (name == "GPU Cull")
            return [this](const FGPassResources&) { RenderContext c = RenderContext::FromDevice(m_device); if (m_gpuCullPass) m_gpuCullPass->Execute(c); };
        if (name == "Shadow")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                ShadowPassParams p = BuildShadowPassParams(m_currentFrameData, m_currentFrameData->cameraRaw);
                ExecuteShadowPass(c.cmd, p);
            };
        if (name == "GBuffer")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                RenderOpaqueGeometryToGBuffer(c.cmd, *m_currentFrameData, m_currentFrameData->camera, m_lastGpuCullDrawListCount);
                if (c.cmd) c.cmd->SetRenderTarget(nullptr);
            };
            if (name == "Copy depth for occlusion")
                return [this](const FGPassResources&) {
                    if (m_prevFrameDepthRT && m_gbuffer && m_gbuffer->GetRenderTargetRHI())
                        m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_prevFrameDepthRT);
                    m_havePrevDepth = (m_prevFrameDepthRT != nullptr);
                };
            if (name == "Lighting")
                return [this](const FGPassResources&) {
                    if (!m_currentFrameData) return;
                    RenderContext c = RenderContext::FromDevice(m_device);
                    DeferredLightingPassParams p =
                        BuildDeferredLightingPassParams(m_currentFrameData, m_currentFrameData->camera, { 0.f, 0.f, 0.f, 1.f });
                    ExecuteDeferredLightingPass(c.cmd, p);
                };
            if (name == "Transparency")
                return [this](const FGPassResources&) {
                    if (!m_currentFrameData) return;
                    RenderContext c = RenderContext::FromDevice(m_device);
                    TransparencyPassParams p = BuildTransparencyPassParams(m_currentFrameData, m_currentFrameData->camera);
                    ExecuteTransparencyPass(c.cmd, p);
                };
        return [](const FGPassResources&) {};
    };
    AddPassesFromRecipe(*m_frameGraph, recipe, nameToHandle, getExecutor);
    AddSkyboxPassToFrameGraph(nameToHandle["HDR"], true);
}

void RenderPipeline::BuildAndExecuteFrameGraph(const IRenderSceneView& view) {
    if (!m_frameGraph || !m_gbuffer || !m_cascadeShadowMap || !m_gbufferPass || !m_shadowPass || !m_lightingPass) return;

    m_currentView = &view;
    PrepareFrameData(view);
    debug::Profiler::SetFrameCounter("visibleMeshes",
        static_cast<uint64_t>(m_frameData.visibleOpaque.size() + m_frameData.visibleTransparent.size()));
    RenderContext ctx = RenderContext::FromDevice(m_device);
    if (ctx.cmd) ctx.cmd->SetRenderTarget(nullptr);
    m_lastGpuCullDrawListCount = PrepareGpuCullFromFrameData(m_frameData, m_frameData.camera);
    m_currentFrameData = &m_frameData;
    PreparePersistentShaderReadInputs(ctx.cmd, m_frameData);

    CompileFrameGraphIfNeeded(PipelineMode::Deferred, false);
    m_frameGraph->Execute(ctx);
}

void RenderPipeline::BuildForwardGraph() {
    if (!m_frameGraph || !m_cascadeShadowMap || !m_hdrRenderTarget) return;
    FGNameToHandle nameToHandle;
    rhi::ITexture* hdrColorTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
    nameToHandle["HDR"] = m_frameGraph->ImportTextureRHI("HDR", hdrColorTex, m_width, m_height);
    nameToHandle["Shadow"] = m_frameGraph->ImportTextureRHI("Shadow", m_cascadeShadowMap->GetDepthTextureRHI(), m_cascadeShadowMap->GetResolution(), m_cascadeShadowMap->GetResolution());

    FrameGraphRecipe recipe;
    if (m_gpuCullPass && m_gpuCullPass->IsSupported(m_device))
        recipe.push_back({ "GPU Cull", {}, {}, true });
    recipe.push_back({ "Shadow", {}, { "Shadow" }, true });
    recipe.push_back({ "Forward Opaque", {}, { "HDR" }, true });
    recipe.push_back({ "Copy depth for occlusion", { "HDR" }, {}, true });
    recipe.push_back({ "Transparency", {}, { "HDR" }, true });

    auto getExecutor = [this](const std::string& name) -> FrameGraph::ExecuteFunc {
        if (name == "GPU Cull")
            return [this](const FGPassResources&) {
                RenderContext c = RenderContext::FromDevice(m_device); if (m_gpuCullPass) m_gpuCullPass->Execute(c);
            };
        if (name == "Shadow")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                ShadowPassParams p = BuildShadowPassParams(m_currentFrameData, m_currentFrameData->cameraRaw);
                ExecuteShadowPass(c.cmd, p);
            };
        if (name == "Forward Opaque")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                IBL* iblPtr = EnsureIBLFromFrameData(*m_currentFrameData);
                const rhi::ClearValue clearFwd = { 0.f, 0.f, 0.f, 1.f };
                if (c.cmd && m_hdrRenderTarget)
                    rhi::ClearAndSetRenderTargetWithViewport(c.cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearFwd);
                if (c.cmd) { c.cmd->SetDepthTest(true); c.cmd->SetCullFace(false); }
                RenderForwardOpaque(c.cmd, *m_currentFrameData);
                if (c.cmd) c.cmd->SetRenderTarget(nullptr);
            };
        if (name == "Copy depth for occlusion")
            return [this](const FGPassResources&) {
                if (m_prevFrameDepthRT && m_hdrRenderTarget)
                    m_device->BlitDepthOnly(m_hdrRenderTarget, m_prevFrameDepthRT);
                m_havePrevDepth = (m_prevFrameDepthRT != nullptr);
            };
        if (name == "Transparency")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                TransparencyPassParams p = BuildTransparencyPassParams(m_currentFrameData, m_currentFrameData->camera);
                ExecuteTransparencyPass(c.cmd, p);
            };
        return [](const FGPassResources&) {};
    };
    AddPassesFromRecipe(*m_frameGraph, recipe, nameToHandle, getExecutor);
    AddSkyboxPassToFrameGraph(nameToHandle["HDR"], false);
}

void RenderPipeline::BuildForwardPlusGraph() {
    if (!m_frameGraph || !m_cascadeShadowMap || !m_hdrRenderTarget) return;
    FGNameToHandle nameToHandle;
    rhi::ITexture* hdrColorTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
    nameToHandle["HDR"] = m_frameGraph->ImportTextureRHI("HDR", hdrColorTex, m_width, m_height);
    nameToHandle["Shadow"] = m_frameGraph->ImportTextureRHI("Shadow", m_cascadeShadowMap->GetDepthTextureRHI(), m_cascadeShadowMap->GetResolution(), m_cascadeShadowMap->GetResolution());

    FrameGraphRecipe recipe;
    recipe.push_back({ "Shadow", {}, { "Shadow" }, true });
    recipe.push_back({ "Forward+ Opaque", {}, { "HDR" }, true });
    recipe.push_back({ "Transparency", {}, { "HDR" }, true });

    auto getExecutor = [this](const std::string& name) -> FrameGraph::ExecuteFunc {
        if (name == "Shadow")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                ShadowPassParams p = BuildShadowPassParams(m_currentFrameData, m_currentFrameData->cameraRaw);
                ExecuteShadowPass(c.cmd, p);
            };
        if (name == "Forward+ Opaque")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                PrepareForwardPlusFrame(c.cmd, *m_currentFrameData);
                RenderForwardPlusOpaqueBatched(c.cmd, *m_currentFrameData);
                if (c.cmd) c.cmd->SetRenderTarget(nullptr);
            };
        if (name == "Transparency")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                TransparencyPassParams p = BuildTransparencyPassParams(m_currentFrameData, m_currentFrameData->camera);
                ExecuteTransparencyPass(c.cmd, p);
            };
        return [](const FGPassResources&) {};
    };
    AddPassesFromRecipe(*m_frameGraph, recipe, nameToHandle, getExecutor);
    AddSkyboxPassToFrameGraph(nameToHandle["HDR"], false);
}

void RenderPipeline::BuildAndExecuteForwardFrameGraph(const IRenderSceneView& view) {
    if (!m_frameGraph || !m_cascadeShadowMap || !m_shadowPass || !m_hdrRenderTarget) return;

    m_currentView = &view;
    PrepareFrameData(view);
    debug::Profiler::SetFrameCounter("visibleMeshes",
        static_cast<uint64_t>(m_frameData.visibleOpaque.size() + m_frameData.visibleTransparent.size()));
    RenderContext ctx = RenderContext::FromDevice(m_device);
    if (ctx.cmd) ctx.cmd->SetRenderTarget(nullptr);
    m_lastGpuCullDrawListCount = PrepareGpuCullFromFrameData(m_frameData, m_frameData.camera);
    m_currentFrameData = &m_frameData;
    PreparePersistentShaderReadInputs(ctx.cmd, m_frameData);

    CompileFrameGraphIfNeeded(PipelineMode::Forward, false);
    m_frameGraph->Execute(ctx);
}

void RenderPipeline::BuildAndExecuteForwardPlusFrameGraph(const IRenderSceneView& view) {
    if (!m_frameGraph || !m_cascadeShadowMap || !m_shadowPass || !m_hdrRenderTarget || !m_clusteredLightGrid) return;

    m_currentView = &view;
    PrepareFrameData(view);
    debug::Profiler::SetFrameCounter("visibleMeshes",
        static_cast<uint64_t>(m_frameData.visibleOpaque.size() + m_frameData.visibleTransparent.size()));
    RenderContext ctx = RenderContext::FromDevice(m_device);
    if (ctx.cmd) ctx.cmd->SetRenderTarget(nullptr);
    m_currentFrameData = &m_frameData;
    PreparePersistentShaderReadInputs(ctx.cmd, m_frameData);

    CompileFrameGraphIfNeeded(PipelineMode::ForwardPlus, false);
    m_frameGraph->Execute(ctx);
}

void RenderPipeline::EnsureForwardResourceSets() {
    if (!m_device) return;
    if (m_forwardFrameSets[0] && m_forwardPassSets[0] && m_forwardTransparentPassSets[0] && m_forwardSceneBuffersSets[0] &&
        m_forwardTransparentSceneBuffersSets[0]) {
        SelectForwardResourceSetFrame();
        return;
    }
    using RT = rhi::IDevice::ResourceSetType;
    rhi::IPipelineLayout* frameLayout = m_device->GetResourceSetLayout(RT::Frame);
    rhi::IPipelineLayout* forwardPassLayout = m_device->GetResourceSetLayout(RT::ForwardPass);
    rhi::IPipelineLayout* materialLayout = m_device->GetResourceSetLayout(RT::Material);
    rhi::IPipelineLayout* sceneLayout = m_device->GetResourceSetLayout(RT::SceneBuffers);
    rhi::IPipelineLayout* deferredLightingLayout = m_device->GetResourceSetLayout(RT::DeferredLightingPass);
    if (!frameLayout || !forwardPassLayout || !materialLayout || !sceneLayout) return;
    rhi::BufferDesc uboDesc;
    uboDesc.sizeInBytes = 320;  // view, projection, viewProj, prevView, prevProjection (5 mat4) for GBuffer velocity
    uboDesc.usage = rhi::BufferUsage::Uniform;
    uboDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;
    rhi::BufferDesc bonesDesc;
    bonesDesc.sizeInBytes = 128 * 64;  // 128 mat4 for skinned
    bonesDesc.usage = rhi::BufferUsage::Storage;
    bonesDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;

    for (uint32_t i = 0; i < kForwardResourceSetsFrames; ++i) {
        m_forwardFrameSets[i] = m_device->CreateDescriptorSet(frameLayout);
        m_forwardPassSets[i] = m_device->CreateDescriptorSet(forwardPassLayout);
        m_forwardTransparentPassSets[i] = m_device->CreateDescriptorSet(forwardPassLayout);
        m_forwardMaterialSets[i] = m_device->CreateDescriptorSet(materialLayout);
        m_forwardSceneBuffersSets[i] = m_device->CreateDescriptorSet(sceneLayout);
        m_forwardTransparentSceneBuffersSets[i] = m_device->CreateDescriptorSet(sceneLayout);
        if (!m_forwardFrameSets[i] || !m_forwardPassSets[i] || !m_forwardTransparentPassSets[i] || !m_forwardMaterialSets[i] ||
            !m_forwardSceneBuffersSets[i] || !m_forwardTransparentSceneBuffersSets[i]) {
            for (rhi::IDescriptorSet*& s : m_forwardFrameSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
            for (rhi::IDescriptorSet*& s : m_forwardPassSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
            for (rhi::IDescriptorSet*& s : m_forwardTransparentPassSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
            for (rhi::IDescriptorSet*& s : m_forwardMaterialSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
            for (rhi::IDescriptorSet*& s : m_forwardSceneBuffersSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
            for (rhi::IDescriptorSet*& s : m_forwardTransparentSceneBuffersSets) { if (s) { m_device->DestroyDescriptorSet(s); s = nullptr; } }
            return;
        }

        m_forwardFrameUBOs[i] = m_device->CreateBuffer(uboDesc, nullptr);
        if (m_forwardFrameUBOs[i]) {
            m_forwardFrameSets[i]->UpdateUniformBuffer(0, m_forwardFrameUBOs[i], 0, 320);
            m_forwardFrameSets[i]->UpdateUniformBuffer(1, m_forwardFrameUBOs[i], 0, 320);
        }
        m_forwardBonesBuffers[i] = m_device->CreateBuffer(bonesDesc, nullptr);

        if (deferredLightingLayout)
            m_deferredLightingPassSets[i] = m_device->CreateDescriptorSet(deferredLightingLayout);
    }

    SelectForwardResourceSetFrame();
    if (m_gbufferPass) {
        m_gbufferPass->SetResourceSpaceSets(m_forwardFrameSet, m_forwardMaterialSet, m_forwardSceneBuffersSet);
        if (m_forwardBonesBuffer) m_gbufferPass->SetBonesBuffer(m_forwardBonesBuffer);
    }
    if (m_lightingPass)
        m_lightingPass->SetResourceSpaceSets(m_forwardFrameSet, m_deferredLightingPassSet);
}

void RenderPipeline::SelectForwardResourceSetFrame() {
    if (!m_device) return;
    const uint32_t frameIndex = m_device->GetCurrentFrameIndex() % kForwardResourceSetsFrames;
    m_forwardFrameSet = m_forwardFrameSets[frameIndex];
    m_forwardPassSet = m_forwardPassSets[frameIndex];
    m_forwardTransparentPassSet = m_forwardTransparentPassSets[frameIndex];
    m_forwardMaterialSet = m_forwardMaterialSets[frameIndex];
    m_forwardSceneBuffersSet = m_forwardSceneBuffersSets[frameIndex];
    m_forwardTransparentSceneBuffersSet = m_forwardTransparentSceneBuffersSets[frameIndex];
    m_deferredLightingPassSet = m_deferredLightingPassSets[frameIndex];
    m_forwardFrameUBO = m_forwardFrameUBOs[frameIndex];
    m_forwardBonesBuffer = m_forwardBonesBuffers[frameIndex];
    m_forwardMaterialDrawSetCursor[frameIndex] = 0;
    m_forwardPassDrawSetCursor[frameIndex] = 0;
    m_forwardSceneBuffersDrawSetCursor[frameIndex] = 0;
    m_deferredLightingDrawSetCursor[frameIndex] = 0;
    if (m_gbufferPass) {
        m_gbufferPass->SetResourceSpaceSets(m_forwardFrameSet, m_forwardMaterialSet, m_forwardSceneBuffersSet);
        if (m_forwardBonesBuffer) m_gbufferPass->SetBonesBuffer(m_forwardBonesBuffer);
    }
    if (m_lightingPass)
        m_lightingPass->SetResourceSpaceSets(m_forwardFrameSet, m_deferredLightingPassSet);
}

void RenderPipeline::EnsureForwardMaterialFallbackTextures() {
    if (!m_device) return;
    if (!m_forwardFallbackAlbedoTexture) {
        rhi::TextureDesc desc = {};
        desc.type = rhi::TextureType::Texture2D;
        desc.width = 1;
        desc.height = 1;
        desc.format = rhi::RHITextureFormat::RGBA8;
        desc.usage = rhi::TextureUsage::ShaderResource;
        desc.mipLevels = 1;
        static const uint8_t white[4] = { 255, 255, 255, 255 };
        m_forwardFallbackAlbedoTexture = m_device->CreateTexture(desc, white);
    }
    if (!m_forwardFallbackNormalTexture) {
        rhi::TextureDesc desc = {};
        desc.type = rhi::TextureType::Texture2D;
        desc.width = 1;
        desc.height = 1;
        desc.format = rhi::RHITextureFormat::RGBA8;
        desc.usage = rhi::TextureUsage::ShaderResource;
        desc.mipLevels = 1;
        static const uint8_t normal[4] = { 128, 128, 255, 255 };
        m_forwardFallbackNormalTexture = m_device->CreateTexture(desc, normal);
    }
    if (!m_forwardFallbackMetalRoughTexture) {
        rhi::TextureDesc desc = {};
        desc.type = rhi::TextureType::Texture2D;
        desc.width = 1;
        desc.height = 1;
        desc.format = rhi::RHITextureFormat::RGBA8;
        desc.usage = rhi::TextureUsage::ShaderResource;
        desc.mipLevels = 1;
        // R=metallic(0), G=roughness(0.5), B/A unused for fallback.
        static const uint8_t mr[4] = { 0, 128, 0, 255 };
        m_forwardFallbackMetalRoughTexture = m_device->CreateTexture(desc, mr);
    }
}

void RenderPipeline::UpdateForwardMaterialSetWithFallback(rhi::IDescriptorSet* targetSet, const Material* material, const MaterialInstance* materialInstance) {
    if (!m_device || !targetSet) return;
    EnsureForwardMaterialFallbackTextures();
    if (!material && !materialInstance) return;

    rhi::IBuffer* matBuf = materialInstance ? materialInstance->GetMaterialBufferRHI(m_device)
                                            : material->GetMaterialBufferRHI(m_device);
    if (matBuf)
        targetSet->UpdateUniformBuffer(0, matBuf, 0, static_cast<uint32_t>(kMaterialBlockStd140Size));

    auto getTex = [&](const std::string& name) -> rhi::ITexture* {
        if (materialInstance) return materialInstance->GetTextureRHI(name);
        return material ? material->GetTextureRHI(name) : nullptr;
    };

    rhi::ITexture* albedo = getTex("u_AlbedoTexture");
    rhi::ITexture* normal = getTex("u_NormalTexture");
    rhi::ITexture* metalRough = getTex("u_MetallicRoughnessTexture");
    if (!metalRough) metalRough = getTex("u_MetallicTexture");

    targetSet->UpdateTexture(1, albedo ? albedo : m_forwardFallbackAlbedoTexture);
    targetSet->UpdateTexture(2, normal ? normal : m_forwardFallbackNormalTexture);
    targetSet->UpdateTexture(3, metalRough ? metalRough : m_forwardFallbackMetalRoughTexture);
}

rhi::IDescriptorSet* RenderPipeline::AcquireForwardDrawMaterialSet() {
    if (!m_device) return nullptr;
    const uint32_t frameIndex = m_device->GetCurrentFrameIndex() % kForwardResourceSetsFrames;
    auto& drawSets = m_forwardMaterialDrawSets[frameIndex];
    uint32_t& cursor = m_forwardMaterialDrawSetCursor[frameIndex];
    if (cursor >= drawSets.size()) {
        rhi::IPipelineLayout* materialLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::Material);
        if (!materialLayout) return nullptr;
        rhi::IDescriptorSet* newSet = m_device->CreateDescriptorSet(materialLayout);
        if (!newSet) return nullptr;
        drawSets.push_back(newSet);
    }
    return drawSets[cursor++];
}

rhi::IDescriptorSet* RenderPipeline::AcquireForwardDrawPassSet() {
    if (!m_device) return nullptr;
    const uint32_t frameIndex = m_device->GetCurrentFrameIndex() % kForwardResourceSetsFrames;
    auto& drawSets = m_forwardPassDrawSets[frameIndex];
    uint32_t& cursor = m_forwardPassDrawSetCursor[frameIndex];
    if (cursor >= drawSets.size()) {
        rhi::IPipelineLayout* passLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::ForwardPass);
        if (!passLayout) return nullptr;
        rhi::IDescriptorSet* newSet = m_device->CreateDescriptorSet(passLayout);
        if (!newSet) return nullptr;
        drawSets.push_back(newSet);
    }
    return drawSets[cursor++];
}

rhi::IDescriptorSet* RenderPipeline::AcquireForwardDrawSceneBuffersSet() {
    if (!m_device) return nullptr;
    const uint32_t frameIndex = m_device->GetCurrentFrameIndex() % kForwardResourceSetsFrames;
    auto& drawSets = m_forwardSceneBuffersDrawSets[frameIndex];
    uint32_t& cursor = m_forwardSceneBuffersDrawSetCursor[frameIndex];
    if (cursor >= drawSets.size()) {
        rhi::IPipelineLayout* sceneLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::SceneBuffers);
        if (!sceneLayout) return nullptr;
        rhi::IDescriptorSet* newSet = m_device->CreateDescriptorSet(sceneLayout);
        if (!newSet) return nullptr;
        drawSets.push_back(newSet);
    }
    return drawSets[cursor++];
}

rhi::IDescriptorSet* RenderPipeline::AcquireDeferredLightingDrawSet() {
    if (!m_device) return nullptr;
    const uint32_t frameIndex = m_device->GetCurrentFrameIndex() % kForwardResourceSetsFrames;
    auto& drawSets = m_deferredLightingDrawSets[frameIndex];
    uint32_t& cursor = m_deferredLightingDrawSetCursor[frameIndex];
    if (cursor >= drawSets.size()) {
        rhi::IPipelineLayout* layout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::DeferredLightingPass);
        if (!layout) return nullptr;
        rhi::IDescriptorSet* newSet = m_device->CreateDescriptorSet(layout);
        if (!newSet) return nullptr;
        drawSets.push_back(newSet);
    }
    return drawSets[cursor++];
}

void RenderPipeline::BuildForwardPassSharedState(const RenderFrameData& frameData, const CameraView& cam, rhi::IDescriptorSet* passSet,
                                                 rhi::IDescriptorSet* sceneBuffersSet, bool usePrevViewProj) {
    if (!passSet || !sceneBuffersSet || !m_forwardFrameUBO) return;
    const LightParams& light = frameData.light;
    IBL* iblPtr = EnsureIBLFromFrameData(frameData);
    const ReflectionProbe* envProbe = m_reflectionProbeManager ? m_reflectionProbeManager->FindClosestProbe(cam.viewPos) : nullptr;
    if (!envProbe && m_reflectionProbeManager) envProbe = m_reflectionProbeManager->GetGlobalProbe();
    const bool useShadows = (m_cascadeShadowMap != nullptr && light.castShadows);
    math::Mat4 lightSpaceMatrix(1.0f);
    if (light.castShadows && m_cascadeShadowMap && m_cascadeShadowMap->GetSplitCount() > 0)
        lightSpaceMatrix = m_cascadeShadowMap->GetLightSpaceMatrix(0);
    FillSceneLightingBuffer(m_sceneLightingBuffer, light, cam.viewPos, lightSpaceMatrix, 0.005f,
        light.castShadows, &frameData.pointLights, &frameData.spotLights, m_cascadeShadowMap.get());

    math::Mat4 viewProj = cam.projection * cam.view;
    float frameUbo[5 * 16];
    std::memcpy(frameUbo, &cam.view[0][0], 16 * sizeof(float));
    std::memcpy(frameUbo + 16, &cam.projection[0][0], 16 * sizeof(float));
    std::memcpy(frameUbo + 32, &viewProj[0][0], 16 * sizeof(float));
    if (usePrevViewProj) {
        std::memcpy(frameUbo + 48, &m_prevView[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 64, &m_prevProjection[0][0], 16 * sizeof(float));
    } else {
        std::memcpy(frameUbo + 48, &cam.view[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 64, &cam.projection[0][0], 16 * sizeof(float));
    }
    m_forwardFrameUBO->Update(frameUbo, 0, 320);

    if (useShadows && m_cascadeShadowMap) passSet->UpdateTexture(0, m_cascadeShadowMap->GetDepthTextureRHI());
    if (m_sceneLightingBuffer) {
        passSet->UpdateStorageBuffer(1, m_sceneLightingBuffer, 0, 0);
        rhi::IBuffer* clusterOrDummy = m_lightIndexBuffer ? m_lightIndexBuffer : m_sceneLightingBuffer;
        passSet->UpdateStorageBuffer(2, clusterOrDummy, 0, 0);
        rhi::IBuffer* sceneFallback = m_sceneLightingBuffer;
        rhi::IBuffer* bonesOrFallback = m_forwardBonesBuffer ? m_forwardBonesBuffer : sceneFallback;
        sceneBuffersSet->UpdateStorageBuffer(0, sceneFallback, 0, 0);
        sceneBuffersSet->UpdateStorageBuffer(1, bonesOrFallback, 0, 0);
        sceneBuffersSet->UpdateStorageBuffer(2, m_sceneLightingBuffer, 0, 0);
        sceneBuffersSet->UpdateStorageBuffer(3, clusterOrDummy, 0, 0);
    }

    rhi::ITexture* irradiance = envProbe && envProbe->IsValid() ? envProbe->GetIrradianceMap() : (iblPtr && iblPtr->IsValid() ? iblPtr->GetIrradianceTextureRHI() : nullptr);
    rhi::ITexture* prefilter = envProbe && envProbe->IsValid() ? envProbe->GetPrefilteredMap() : (iblPtr && iblPtr->IsValid() ? iblPtr->GetPrefilterTextureRHI() : nullptr);
    rhi::ITexture* brdfLut = (iblPtr && iblPtr->IsValid()) ? iblPtr->GetBRDFLUTTextureRHI() : nullptr;
    if (irradiance) passSet->UpdateTexture(3, irradiance);
    if (prefilter) passSet->UpdateTexture(4, prefilter);
    if (brdfLut) passSet->UpdateTexture(5, brdfLut);
}

void RenderPipeline::RenderForwardOpaque(rhi::ICommandList* cmd, const RenderFrameData& frameData) {
    RenderForwardOpaqueForCamera(cmd, frameData, frameData.camera, frameData.visibleOpaque);
}

void RenderPipeline::RenderForwardOpaqueForCamera(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam,
                                                  const std::vector<Drawable>& opaqueDrawables) {
    IBL* iblPtr = EnsureIBLFromFrameData(frameData);
    const bool useTypedForwardPush = (m_device && m_device->GetRenderBackend() == ::eyelib::render::RenderBackend::Vulkan);
    const ReflectionProbe* envProbe = m_reflectionProbeManager ? m_reflectionProbeManager->FindClosestProbe(cam.viewPos) : nullptr;
    if (!envProbe && m_reflectionProbeManager) envProbe = m_reflectionProbeManager->GetGlobalProbe();
    EnsureForwardResourceSets();
    rhi::IDescriptorSet* passSetForCall = AcquireForwardDrawPassSet();
    rhi::IDescriptorSet* sceneSetForCall = AcquireForwardDrawSceneBuffersSet();
    rhi::IDescriptorSet* bindlessSet = m_bindlessHeap ? m_bindlessHeap->GetDescriptorSet() : nullptr;
    if (!ForwardPbrResourceBindingsReady(m_device, cmd, m_forwardFrameSet, passSetForCall, m_forwardMaterialSet,
            sceneSetForCall, m_forwardFrameUBO, m_bindlessHeap)) {
        static bool s_loggedForwardSkip = false;
        if (!s_loggedForwardSkip) {
            core::Log::WarnF(
                "RenderForwardOpaque: skipped (need command list, bindless heap, and forward resource-space sets). "
                "Ensure supportsBindless, supportsResourceSpaceBindPath, and successful EnsureForwardResourceSets.");
            s_loggedForwardSkip = true;
        }
        return;
    }

    BuildForwardPassSharedState(frameData, cam, passSetForCall, sceneSetForCall, false);

    if (!m_shaderCache) return;
    std::map<ShaderKey, std::vector<Drawable>> buckets;
    for (const Drawable& d : opaqueDrawables) {
        if (!d.mesh || !d.material) continue;
        const Material* baseMat = d.materialInstance ? d.materialInstance->GetMaterial().get() : d.material.get();
        InstanceType it = (d.bones && d.boneCount > 0) ? InstanceType::Skinned : InstanceType::Static;
        ShaderKey key{ PipelineStage::Forward, it, baseMat->GetMaterialShaderId() };
        buckets[key].push_back(d);
    }
    size_t shaderCompileMiss = 0;
    size_t shaderInvalid = 0;
    size_t pipelineInvalid = 0;
    size_t drawMaterialSetMiss = 0;
    size_t meshHandleInvalid = 0;
    size_t drawSubmitted = 0;

    for (auto& [key, drawables] : buckets) {
        const Material* matForCompile = drawables[0].materialInstance ? drawables[0].materialInstance->GetMaterial().get() : drawables[0].material.get();
        auto shader = m_shaderCache->getOrCompile(m_device, key, matForCompile);
        if (!shader) {
            ++shaderCompileMiss;
            continue;
        }
        if (!shader->IsValid()) {
            ++shaderInvalid;
            continue;
        }
        rhi::IPipeline* pipe = shader->GetPipelineRHI();
        if (pipe && !pipe->HasValidProgram()) {
            ++pipelineInvalid;
            continue;
        }
        Shader* sh = shader.get();
        sh->Bind(m_device);
        debug::Profiler::AddFrameCounter("stateChanges", 1);
        const int useIBL = ((envProbe && envProbe->IsValid()) || (iblPtr && iblPtr->IsValid())) ? 1 : 0;
        if (!useTypedForwardPush)
            sh->SetInt("u_UseIBL", useIBL);
        for (const Drawable& d : drawables) {
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
            if (bonePtr && boneCount > 0) boneCount = std::min(boneCount, kMaxBones);

            rhi::IDescriptorSet* drawMaterialSet = AcquireForwardDrawMaterialSet();
            if (!drawMaterialSet) {
                ++drawMaterialSetMiss;
                continue;
            }
            UpdateForwardMaterialSetWithFallback(drawMaterialSet, d.material.get(), d.materialInstance.get());
            const int useAlbedoTexture = ComputeForwardAlbedoTextureFlag(d.material.get(), d.materialInstance.get());
            if (!useTypedForwardPush)
                sh->SetInt("u_UseAlbedoTexture", useAlbedoTexture);
            rhi::IDescriptorSet* sceneSetForDraw = sceneSetForCall;
            if (bonePtr && boneCount > 0 && m_forwardBonesBuffer) {
                const uint32_t boneBytes = static_cast<uint32_t>(boneCount * sizeof(math::Mat4));
                m_forwardBonesBuffer->Update(bonePtr, 0, boneBytes);
                rhi::IDescriptorSet* drawSceneSet = AcquireForwardDrawSceneBuffersSet();
                if (drawSceneSet) {
                    // Clone shared scene-buffers bindings and override bones binding for this draw.
                    drawSceneSet->UpdateStorageBuffer(0, m_sceneLightingBuffer, 0, 0);
                    drawSceneSet->UpdateStorageBuffer(1, m_forwardBonesBuffer, 0, boneBytes);
                    drawSceneSet->UpdateStorageBuffer(2, m_sceneLightingBuffer, 0, 0);
                    rhi::IBuffer* clusterOrDummy = m_lightIndexBuffer ? m_lightIndexBuffer : m_sceneLightingBuffer;
                    drawSceneSet->UpdateStorageBuffer(3, clusterOrDummy, 0, 0);
                    sceneSetForDraw = drawSceneSet;
                }
            }
            rhi::IDescriptorSet* sets[5] = { m_forwardFrameSet, passSetForCall, drawMaterialSet, bindlessSet, sceneSetForDraw };
            cmd->BindDescriptorSets(0, 5, sets);
            if (useTypedForwardPush) {
                abi::WriteForwardPushConstants(cmd, d.transform, useIBL, useAlbedoTexture);
            } else {
                sh->SetMat4("u_Model", d.transform);
                sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
            }
            debug::Profiler::AddFrameCounter("drawCalls", 1);
            debug::Profiler::AddFrameCounter("triangles", GetIndexCountForDraw(d.mesh.get(), d.submeshIndex) / 3u);
            d.mesh->Bind(m_device);
            {
                MeshHandle h = d.mesh->GetHandle();
                if (h.IsValid()) {
                    h.Draw(m_device, d.submeshIndex);
                    ++drawSubmitted;
                } else {
                    ++meshHandleInvalid;
                }
            }
            d.mesh->Unbind();
        }
        sh->Unbind();
    }

    const bool hasVisible = !opaqueDrawables.empty();
    const bool suspicious = hasVisible && drawSubmitted == 0;
    if (suspicious) {
        core::Log::WarnF(
            "ForwardOpaqueSubmit: visible=%zu buckets=%zu draws=%zu shaderMiss=%zu shaderInvalid=%zu pipeInvalid=%zu drawSetMiss=%zu meshHandleInvalid=%zu",
            opaqueDrawables.size(), buckets.size(), drawSubmitted,
            shaderCompileMiss, shaderInvalid, pipelineInvalid, drawMaterialSetMiss, meshHandleInvalid);
    }
}

void RenderPipeline::RenderForwardTransparent(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam) {
    if (!cmd || !m_shaderCache) return;
    IBL* iblPtr = EnsureIBLFromFrameData(frameData);
    const ReflectionProbe* envProbe = m_reflectionProbeManager ? m_reflectionProbeManager->FindClosestProbe(cam.viewPos) : nullptr;
    if (!envProbe && m_reflectionProbeManager) envProbe = m_reflectionProbeManager->GetGlobalProbe();
    EnsureForwardResourceSets();
    rhi::IDescriptorSet* passSetForCall = AcquireForwardDrawPassSet();
    rhi::IDescriptorSet* sceneSetForCall = AcquireForwardDrawSceneBuffersSet();
    rhi::IDescriptorSet* bindlessSet = m_bindlessHeap ? m_bindlessHeap->GetDescriptorSet() : nullptr;
    if (!ForwardPbrResourceBindingsReady(m_device, cmd, m_forwardFrameSet, passSetForCall, m_forwardMaterialSet,
            sceneSetForCall, m_forwardFrameUBO, m_bindlessHeap)) {
        static bool s_loggedTrSkip = false;
        if (!s_loggedTrSkip) {
            core::Log::WarnF("RenderForwardTransparent: skipped (need bindless + forward resource-space sets).");
            s_loggedTrSkip = true;
        }
        return;
    }

    struct TEntry {
        size_t idx;
        float dist;
    };
    std::vector<TEntry> sorted;
    sorted.reserve(frameData.visibleTransparent.size());
    for (size_t i = 0; i < frameData.visibleTransparent.size(); ++i) {
        const Drawable& d = frameData.visibleTransparent[i];
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!d.mesh || !material) continue;
        const float alpha = d.materialInstance ? d.materialInstance->GetAlpha() : material->GetAlpha();
        if (alpha >= 0.99f) continue;
        sorted.push_back({ i, DistanceToCameraTransparent(d.transform, cam.viewPos) });
    }
    std::sort(sorted.begin(), sorted.end(), [](const TEntry& a, const TEntry& b) { return a.dist > b.dist; });
    if (sorted.empty()) return;

    BuildForwardPassSharedState(frameData, cam, passSetForCall, sceneSetForCall, true);

    const int useIBL = ((envProbe && envProbe->IsValid()) || (iblPtr && iblPtr->IsValid())) ? 1 : 0;

    for (const TEntry& e : sorted) {
        const Drawable& d = frameData.visibleTransparent[e.idx];
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!material) continue;
        ApplyMaterialBlendCullTransparent(cmd, material.get());
        const Material* baseMat = material.get();
        InstanceType it = (d.bones && d.boneCount > 0) ? InstanceType::Skinned : InstanceType::Static;
        ShaderKey key{ PipelineStage::Forward, it, baseMat->GetMaterialShaderId() };
        auto shader = m_shaderCache->getOrCompile(m_device, key, baseMat);
        if (!shader || !shader->IsValid()) continue;
        rhi::IPipeline* pipe = shader->GetPipelineRHI();
        if (pipe && !pipe->HasValidProgram()) continue;
        Shader* sh = shader.get();
        sh->Bind(m_device);
        if (d.materialInstance) d.materialInstance->ApplyToShader(m_device, sh, cmd);
        else material->ApplyToShader(m_device, sh, cmd);

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
        if (bonePtr && boneCount > 0) boneCount = std::min(boneCount, kMaxBones);

        rhi::IDescriptorSet* drawMaterialSet = AcquireForwardDrawMaterialSet();
        if (!drawMaterialSet) {
            sh->Unbind();
            continue;
        }
        UpdateForwardMaterialSetWithFallback(drawMaterialSet, d.material.get(), d.materialInstance.get());
        const int useAlbedoTexture = ComputeForwardAlbedoTextureFlag(d.material.get(), d.materialInstance.get());
        rhi::IDescriptorSet* sceneSetForDraw = sceneSetForCall;
        if (bonePtr && boneCount > 0 && m_forwardBonesBuffer) {
            const uint32_t boneBytes = static_cast<uint32_t>(boneCount * sizeof(math::Mat4));
            m_forwardBonesBuffer->Update(bonePtr, 0, boneBytes);
            rhi::IDescriptorSet* drawSceneSet = AcquireForwardDrawSceneBuffersSet();
            if (drawSceneSet) {
                drawSceneSet->UpdateStorageBuffer(0, m_sceneLightingBuffer, 0, 0);
                drawSceneSet->UpdateStorageBuffer(1, m_forwardBonesBuffer, 0, boneBytes);
                drawSceneSet->UpdateStorageBuffer(2, m_sceneLightingBuffer, 0, 0);
                rhi::IBuffer* clusterOrDummy = m_lightIndexBuffer ? m_lightIndexBuffer : m_sceneLightingBuffer;
                drawSceneSet->UpdateStorageBuffer(3, clusterOrDummy, 0, 0);
                sceneSetForDraw = drawSceneSet;
            }
        }
        rhi::IDescriptorSet* sets[5] = { m_forwardFrameSet, passSetForCall, drawMaterialSet, bindlessSet, sceneSetForDraw };
        cmd->BindDescriptorSets(0, 5, sets);
        abi::WriteForwardPushConstants(cmd, d.transform, useIBL, useAlbedoTexture);
        debug::Profiler::AddFrameCounter("drawCalls", 1);
        debug::Profiler::AddFrameCounter("triangles", GetIndexCountForDraw(d.mesh.get(), d.submeshIndex) / 3u);
        debug::Profiler::AddFrameCounter("stateChanges", 1);
        d.mesh->Bind(m_device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device, d.submeshIndex); }
        d.mesh->Unbind();
        sh->Unbind();
    }

    cmd->SetDepthWrite(true);
    cmd->SetBlend(false);
    cmd->SetCullFace(true);
}

void RenderPipeline::RenderForwardOpaqueStereo(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& leftCam, const CameraView& rightCam) {
    if (m_device && m_device->GetRenderBackend() == RenderBackend::Vulkan && m_shaderCache) {
        math::Frustum leftFrustum;
        math::Frustum rightFrustum;
        leftFrustum.FromMatrix(leftCam.projection * leftCam.view);
        rightFrustum.FromMatrix(rightCam.projection * rightCam.view);
        std::vector<Drawable> leftOpaque;
        std::vector<Drawable> rightOpaque;
        leftOpaque.reserve(frameData.allOpaque.size());
        rightOpaque.reserve(frameData.allOpaque.size());
        for (const Drawable& d : frameData.allOpaque) {
            if (!d.mesh || !d.material) continue;
            if (IsForwardDrawableVisible(d, leftFrustum)) leftOpaque.push_back(d);
            if (IsForwardDrawableVisible(d, rightFrustum)) rightOpaque.push_back(d);
        }
        const uint32_t halfW = m_width / 2;
        if (cmd) {
            cmd->SetViewport(0, 0, halfW, m_height);
            RenderForwardOpaqueForCamera(cmd, frameData, leftCam, leftOpaque);
            cmd->SetViewport(halfW, 0, halfW, m_height);
            RenderForwardOpaqueForCamera(cmd, frameData, rightCam, rightOpaque);
        }
        return;
    }

    EnsureStereoForwardShader();
    if (!m_stereoForwardShader || !m_stereoForwardShader->IsValid()) return;
    math::Frustum leftFrustum;
    math::Frustum rightFrustum;
    leftFrustum.FromMatrix(leftCam.projection * leftCam.view);
    rightFrustum.FromMatrix(rightCam.projection * rightCam.view);

    const LightParams& light = frameData.light;
    const int kShadowMapUnit = static_cast<int>(rhi::PassBindings::ShadowMap);
    const bool useShadows = (m_cascadeShadowMap != nullptr && light.castShadows);
    if (useShadows && cmd) {
        rhi::ITexture* shadowTex = m_cascadeShadowMap->GetDepthTextureRHI();
        if (shadowTex) cmd->BindTexture(shadowTex, kShadowMapUnit);
    }

    const uint32_t halfW = m_width / 2;
    if (cmd) {
        cmd->SetViewportIndexed(0, 0, 0, halfW, m_height);
        cmd->SetViewportIndexed(1, halfW, 0, halfW, m_height);
    }

    const int kMaxPointLights = 8;
    const int kMaxSpotLights = 4;
    const int numPoint = static_cast<int>(std::min(frameData.pointLights.size(), size_t(kMaxPointLights)));
    const int numSpot = static_cast<int>(std::min(frameData.spotLights.size(), size_t(kMaxSpotLights)));

    math::Mat4 viewArr[2] = { leftCam.view, rightCam.view };
    math::Mat4 projArr[2] = { leftCam.projection, rightCam.projection };

    FillSceneLightingBuffer(m_sceneLightingBuffer, light, leftCam.viewPos, m_cascadeShadowMap && m_cascadeShadowMap->GetSplitCount() > 0 ? m_cascadeShadowMap->GetLightSpaceMatrix(0) : math::Mat4(1.0f), 0.005f,
        light.castShadows, &frameData.pointLights, &frameData.spotLights, m_cascadeShadowMap.get());
    if (cmd && m_sceneLightingBuffer)
        cmd->BindStorageBuffer(m_sceneLightingBuffer, 5, 0, 0);

    for (const Drawable& d : frameData.allOpaque) {
        if (!d.mesh || !d.material) continue;
        if (!IsForwardDrawableVisible(d, leftFrustum) && !IsForwardDrawableVisible(d, rightFrustum)) continue;
        Material* mat = (d.materialInstance ? d.materialInstance->GetMaterial().get() : d.material.get());
        if (!mat) continue;

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
            sh->SetMat4("u_LightSpaceMatrix", m_cascadeShadowMap->GetSplitCount() > 0 ? m_cascadeShadowMap->GetLightSpaceMatrix(0) : math::Mat4(1.0f));
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
            const auto& p = frameData.pointLights[static_cast<size_t>(i)];
            sh->SetVec3("u_PointLightPos[" + std::to_string(i) + "]", p.position);
            sh->SetVec3("u_PointLightColor[" + std::to_string(i) + "]", p.color);
            sh->SetFloat("u_PointLightIntensity[" + std::to_string(i) + "]", p.intensity);
            sh->SetFloat("u_PointLightRadius[" + std::to_string(i) + "]", p.radius);
        }
        sh->SetInt("u_SpotLightCount", numSpot);
        for (int i = 0; i < numSpot; ++i) {
            const auto& s = frameData.spotLights[static_cast<size_t>(i)];
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
    }

    if (useShadows && cmd) cmd->UnbindTexture(kShadowMapUnit);
}

void RenderPipeline::BuildForwardStereoGraph() {
    if (!m_frameGraph || !m_cascadeShadowMap || !m_hdrRenderTarget) return;
    FGNameToHandle nameToHandle;
    rhi::ITexture* hdrColorTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
    nameToHandle["HDR"] = m_frameGraph->ImportTextureRHI("HDR", hdrColorTex, m_width, m_height);
    nameToHandle["Shadow"] = m_frameGraph->ImportTextureRHI("Shadow", m_cascadeShadowMap->GetDepthTextureRHI(), m_cascadeShadowMap->GetResolution(), m_cascadeShadowMap->GetResolution());

    FrameGraphRecipe recipe;
    recipe.push_back({ "Shadow", {}, { "Shadow" }, true });
    recipe.push_back({ "Forward Opaque", {}, { "HDR" }, true });
    recipe.push_back({ "Transparency", {}, { "HDR" }, true });

    auto getExecutor = [this](const std::string& name) -> FrameGraph::ExecuteFunc {
        if (name == "Shadow")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                RunShadowPassBatched(c.cmd, *m_currentFrameData, m_currentLeftCamRaw);
            };
        if (name == "Forward Opaque")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                IBL* iblPtr = EnsureIBLFromFrameData(*m_currentFrameData);
                const rhi::ClearValue clearFwd = { 0.f, 0.f, 0.f, 1.f };
                if (c.cmd && m_hdrRenderTarget)
                    rhi::ClearAndSetRenderTargetWithViewport(c.cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearFwd);
                if (c.cmd) { c.cmd->SetDepthTest(true); c.cmd->SetCullFace(false); }
                RenderForwardOpaqueStereo(c.cmd, *m_currentFrameData, m_currentLeftCam, m_currentRightCam);
            };
        if (name == "Transparency")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                if (c.cmd && m_hdrRenderTarget) {
                    c.cmd->SetRenderTarget(m_hdrRenderTarget);
                    const uint32_t halfW = m_width / 2;
                    c.cmd->SetViewport(0, 0, halfW, m_height);
                }
                if (m_device && m_device->GetRenderBackend() == RenderBackend::Vulkan && m_shaderCache)
                    RenderForwardTransparent(c.cmd, *m_currentFrameData, m_currentLeftCam);
                else
                    TransparencyPass::Render(c, m_currentFrameData->visibleTransparent, m_currentLeftCam.view, m_currentLeftCam.projection, m_currentLeftCam.viewPos, m_shaderCache);
                if (c.cmd && m_hdrRenderTarget) {
                    const uint32_t halfW = m_width / 2;
                    c.cmd->SetViewport(halfW, 0, halfW, m_height);
                }
                if (m_device && m_device->GetRenderBackend() == RenderBackend::Vulkan && m_shaderCache)
                    RenderForwardTransparent(c.cmd, *m_currentFrameData, m_currentRightCam);
                else
                    TransparencyPass::Render(c, m_currentFrameData->visibleTransparent, m_currentRightCam.view, m_currentRightCam.projection, m_currentRightCam.viewPos, m_shaderCache);
                if (c.cmd) c.cmd->SetRenderTarget(nullptr);
            };
        return [](const FGPassResources&) {};
    };
    AddPassesFromRecipe(*m_frameGraph, recipe, nameToHandle, getExecutor);
    m_frameGraph->AddPass("SkyboxLeft", [this](const FGPassResources&) {
        if (!m_currentFrameData) return;
        rhi::ICommandList* cmd = GetRHICommandList();
        if (cmd && m_hdrRenderTarget) { cmd->SetRenderTarget(m_hdrRenderTarget); cmd->SetViewport(0, 0, m_width / 2, m_height); }
        auto skybox = m_currentFrameData->skybox;
        if (skybox && skybox->IsValid())
            RenderSkyboxImpl(m_currentLeftCam, skybox, false);
        else
            RenderProceduralSkyboxImpl(m_currentLeftCam, false);
    }, {}, { nameToHandle["HDR"] }, true);
    m_frameGraph->AddPass("SkyboxRight", [this](const FGPassResources&) {
        if (!m_currentFrameData) return;
        rhi::ICommandList* cmd = GetRHICommandList();
        if (cmd && m_hdrRenderTarget) { cmd->SetRenderTarget(m_hdrRenderTarget); cmd->SetViewport(m_width / 2, 0, m_width / 2, m_height); }
        auto skybox = m_currentFrameData->skybox;
        if (skybox && skybox->IsValid())
            RenderSkyboxImpl(m_currentRightCam, skybox, false);
        else
            RenderProceduralSkyboxImpl(m_currentRightCam, false);
    }, {}, { nameToHandle["HDR"] }, true);
}

void RenderPipeline::BuildAndExecuteForwardFrameGraphStereo(const IRenderSceneView& view, const CameraView& leftCam, const CameraView& rightCam) {
    if (!m_frameGraph || !m_cascadeShadowMap || !m_shadowPass || !m_hdrRenderTarget) return;

    m_currentView = &view;
    m_currentLeftCamRaw = leftCam;
    m_currentRightCamRaw = rightCam;
    m_currentLeftCam = AdjustCameraForBackend(m_device, m_currentLeftCamRaw);
    m_currentRightCam = AdjustCameraForBackend(m_device, m_currentRightCamRaw);
    PrepareFrameData(view);
    m_currentFrameData = &m_frameData;

    RenderContext ctx = RenderContext::FromDevice(m_device);
    if (ctx.cmd) ctx.cmd->SetRenderTarget(nullptr);
    PreparePersistentShaderReadInputs(ctx.cmd, m_frameData);

    CompileFrameGraphIfNeeded(PipelineMode::Forward, true);
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
    // Intentionally do not force GPU sync here; explicit sync is handled by Finish()/fences when needed.

    BuildAndExecuteForwardFrameGraphStereo(view, leftCam, rightCam);

    const CameraView cam = m_currentLeftCam;
    if (m_prePostCallback) {
        const RenderScene* sceneForCb = view.GetSceneForCallback();
        if (sceneForCb)
            m_prePostCallback(m_hdrRenderTarget, m_width, m_height, *sceneForCb);
    }

    if (eyelib::debug::GetPresets().debugDrawEnabled.load() && m_device) {
        RenderContext ctx = RenderContext::FromDevice(m_device);
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
        const ReflectionProbe* probe = m_reflectionProbeManager->FindClosestProbe(cam.viewPos);
        if (!probe) probe = m_reflectionProbeManager->GetGlobalProbe();
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

void RenderPipeline::RenderForward(const IRenderSceneView& view) {
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);
    PrepareFrameData(view);
    rhi::ICommandList* cmd = GetRHICommandList();
    if (cmd && m_hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, cv);
    }
    if (cmd) { cmd->SetDepthTest(true); cmd->SetCullFace(false); }

    RenderForwardOpaque(cmd, m_frameData);

    const CameraView& cam = m_frameData.camera;
    RenderContext ctx = RenderContext::FromDevice(m_device);
    if (m_device && m_device->GetRenderBackend() == RenderBackend::Vulkan && m_shaderCache)
        RenderForwardTransparent(cmd, m_frameData, cam);
    else
        TransparencyPass::Render(ctx, m_frameData.visibleTransparent, cam.view, cam.projection, cam.viewPos, m_shaderCache);
    if (cmd) { cmd->SetCullFace(true); cmd->SetRenderTarget(nullptr); }
}

void RenderPipeline::PrepareForwardPlusFrame(rhi::ICommandList* cmd, const RenderFrameData& frameData) {
    const CameraView& cam = frameData.camera;
    const LightParams& light = frameData.light;
    const std::vector<PointLight>& pointLights = frameData.pointLights;
    const std::vector<SpotLight>& spotLights = frameData.spotLights;

    if (m_clusteredLightGrid->GetNearPlane() != cam.nearPlane ||
        m_clusteredLightGrid->GetFarPlane() != cam.farPlane) {
        m_clusteredLightGrid->Initialize(m_width, m_height, cam.nearPlane, cam.farPlane);
    }

    const size_t kMaxPointLights = 256;
    const size_t kMaxSpotLights = 64;
    const size_t requiredLightIndexBytes = m_clusteredLightGrid->GetLightIndexDataSize();
    if (!pointLights.empty() && m_device) {
        if (!m_pointLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxPointLights * sizeof(PointLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_pointLightBuffer = m_device->CreateBuffer(bd, nullptr);
        }
        if (m_pointLightBuffer) {
            const size_t sz = pointLights.size() * sizeof(PointLight);
            m_pointLightBuffer->Update(pointLights.data(), 0, sz);
        }
    }
    if (!spotLights.empty() && m_device) {
        if (!m_spotLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxSpotLights * sizeof(SpotLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_spotLightBuffer = m_device->CreateBuffer(bd, nullptr);
        }
        if (m_spotLightBuffer) {
            const size_t sz = spotLights.size() * sizeof(SpotLight);
            m_spotLightBuffer->Update(spotLights.data(), 0, sz);
        }
    }
    if (m_device && requiredLightIndexBytes > 0) {
        if (!m_lightIndexBuffer || m_lightIndexBuffer->GetSize() < requiredLightIndexBytes) {
            if (m_lightIndexBuffer) {
                m_device->DestroyBuffer(m_lightIndexBuffer);
                m_lightIndexBuffer = nullptr;
            }
            rhi::BufferDesc bd;
            bd.sizeInBytes = requiredLightIndexBytes;
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_lightIndexBuffer = m_device->CreateBuffer(bd, nullptr);
        }
    }

    // Prefer GPU light culling when supported (zero CPU cost); CPU path is fallback.
    if (m_lightCullPass && m_lightCullPass->IsSupported(m_device) && m_lightIndexBuffer) {
        m_lightCullPass->SetPointLights(m_pointLightBuffer, static_cast<uint32_t>(pointLights.size()));
        m_lightCullPass->SetSpotLights(m_spotLightBuffer, static_cast<uint32_t>(spotLights.size()));
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
        uint32_t zero = 0u;
        m_lightIndexBuffer->Update(&zero, 0, sizeof(zero));
        RenderContext lightCullCtx = RenderContext::FromDevice(m_device);
        m_lightCullPass->Execute(lightCullCtx);
    } else {
        m_clusteredLightGrid->BuildLightGrid(pointLights, spotLights,
                                             cam.view, cam.projection, cam.viewPos);
        m_clusteredLightGrid->UploadToGPU(m_lightIndexBuffer);
    }
    if (cmd && m_hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, cv);
    }
    if (cmd) { cmd->SetDepthTest(true); cmd->SetCullFace(false); }

    math::Mat4 lightSpaceMatrix(1.0f);
    if (light.castShadows && m_cascadeShadowMap && m_cascadeShadowMap->GetSplitCount() > 0)
        lightSpaceMatrix = m_cascadeShadowMap->GetLightSpaceMatrix(0);
    FillSceneLightingBuffer(m_sceneLightingBuffer, light, cam.viewPos, lightSpaceMatrix, 0.005f,
        light.castShadows, &frameData.pointLights, &frameData.spotLights, m_cascadeShadowMap.get());
}

void RenderPipeline::RenderForwardPlusOpaqueBatched(rhi::ICommandList* cmd, const RenderFrameData& frameData) {
    const CameraView& cam = frameData.camera;

    if (!m_shaderCache) return;

    EnsureForwardResourceSets();
    rhi::IDescriptorSet* passSetForCall = AcquireForwardDrawPassSet();
    rhi::IDescriptorSet* sceneSetForCall = AcquireForwardDrawSceneBuffersSet();
    rhi::IDescriptorSet* bindlessSet = m_bindlessHeap ? m_bindlessHeap->GetDescriptorSet() : nullptr;
    if (!ForwardPbrResourceBindingsReady(m_device, cmd, m_forwardFrameSet, passSetForCall, m_forwardMaterialSet,
            sceneSetForCall, m_forwardFrameUBO, m_bindlessHeap)) {
        static bool s_loggedForwardPlusSkip = false;
        if (!s_loggedForwardPlusSkip) {
            core::Log::WarnF(
                "RenderForwardPlusOpaqueBatched: skipped (need command list, bindless heap, and forward resource-space sets).");
            s_loggedForwardPlusSkip = true;
        }
        return;
    }
    IBL* iblPtr = EnsureIBLFromFrameData(frameData);
    const bool useTypedForwardPush = (m_device && m_device->GetRenderBackend() == ::eyelib::render::RenderBackend::Vulkan);
    const ReflectionProbe* envProbe = m_reflectionProbeManager ? m_reflectionProbeManager->FindClosestProbe(cam.viewPos) : nullptr;
    if (!envProbe && m_reflectionProbeManager) envProbe = m_reflectionProbeManager->GetGlobalProbe();
    BuildForwardPassSharedState(frameData, cam, passSetForCall, sceneSetForCall, true);

    std::vector<DrawBatch> batches = BuildBatches(frameData.visibleOpaque);

    for (DrawBatch& batch : batches) {
        if (batch.IsInstanced() && m_instanceMatrixBuffer && batch.transforms.size() <= kMaxInstanceBatchSize && batch.material) {
            ShaderKey key{ PipelineStage::ForwardPlus, InstanceType::Instanced, batch.material->GetMaterialShaderId() };
            auto shader = m_shaderCache->getOrCompile(m_device, key, batch.material.get());
            if (shader && shader->IsValid()) {
                Shader* sh = shader.get();
                sh->Bind(m_device);
                debug::Profiler::AddFrameCounter("stateChanges", 1);
                rhi::IDescriptorSet* drawMaterialSet = AcquireForwardDrawMaterialSet();
                if (!drawMaterialSet) { sh->Unbind(); continue; }
                UpdateForwardMaterialSetWithFallback(drawMaterialSet, batch.material.get(), batch.materialInstance.get());
                const int useAlbedoTexture = ComputeForwardAlbedoTextureFlag(batch.material.get(), batch.materialInstance.get());
                if (!useTypedForwardPush)
                    sh->SetInt("u_UseAlbedoTexture", useAlbedoTexture);
                const size_t byteSize = batch.transforms.size() * sizeof(math::Mat4);
                m_instanceMatrixBuffer->Update(batch.transforms.data(), 0, static_cast<uint32_t>(byteSize));
                rhi::IDescriptorSet* sceneSetForDraw = sceneSetForCall;
                rhi::IDescriptorSet* drawSceneSet = AcquireForwardDrawSceneBuffersSet();
                if (drawSceneSet) {
                    drawSceneSet->UpdateStorageBuffer(0, m_sceneLightingBuffer, 0, 0);
                    drawSceneSet->UpdateStorageBuffer(1, m_instanceMatrixBuffer, 0, static_cast<uint32_t>(byteSize));
                    drawSceneSet->UpdateStorageBuffer(2, m_sceneLightingBuffer, 0, 0);
                    rhi::IBuffer* clusterOrDummy = m_lightIndexBuffer ? m_lightIndexBuffer : m_sceneLightingBuffer;
                    drawSceneSet->UpdateStorageBuffer(3, clusterOrDummy, 0, 0);
                    sceneSetForDraw = drawSceneSet;
                }
                rhi::IDescriptorSet* sets[5] = { m_forwardFrameSet, passSetForCall, drawMaterialSet, bindlessSet, sceneSetForDraw };
                cmd->BindDescriptorSets(0, 5, sets);
                const int useIBL = ((envProbe && envProbe->IsValid()) || (iblPtr && iblPtr->IsValid())) ? 1 : 0;
                if (useTypedForwardPush) {
                    // Forward+ instanced VS reads model from instance buffer; only FS flags are needed.
                    abi::WriteForwardPushConstants(cmd, math::Mat4(1.0f), useIBL, useAlbedoTexture);
                } else {
                    sh->SetInt("u_UseIBL", useIBL);
                }
                debug::Profiler::AddFrameCounter("drawCalls", 1);
                debug::Profiler::AddFrameCounter("triangles", (GetIndexCountForDraw(batch.mesh.get(), batch.submeshIndex) / 3u) * static_cast<uint32_t>(batch.transforms.size()));
                batch.mesh->Bind(m_device);
                { MeshHandle h = batch.mesh->GetHandle(); if (h.IsValid()) h.DrawInstanced(m_device, batch.submeshIndex, static_cast<uint32_t>(batch.transforms.size())); }
                batch.mesh->Unbind();
                sh->Unbind();
            }
        } else {
            for (size_t i = 0; i < batch.transforms.size(); ++i) {
                Drawable single = batch.ToDrawable(i);
                const bool useSkinned = (single.bones != nullptr && single.boneCount > 0);
                const Material* baseMat = single.materialInstance ? single.materialInstance->GetMaterial().get() : single.material.get();
                if (!baseMat) continue;
                ShaderKey key{ PipelineStage::ForwardPlus, useSkinned ? InstanceType::Skinned : InstanceType::Static, baseMat->GetMaterialShaderId() };
                auto shader = m_shaderCache->getOrCompile(m_device, key, baseMat);
                if (!shader || !shader->IsValid()) continue;
                Shader* sh = shader.get();
                sh->Bind(m_device);
                debug::Profiler::AddFrameCounter("stateChanges", 1);
                const int useIBL = ((envProbe && envProbe->IsValid()) || (iblPtr && iblPtr->IsValid())) ? 1 : 0;
                if (!useTypedForwardPush)
                    sh->SetInt("u_UseIBL", useIBL);

                rhi::IDescriptorSet* drawMaterialSet = AcquireForwardDrawMaterialSet();
                if (!drawMaterialSet) { sh->Unbind(); continue; }
                UpdateForwardMaterialSetWithFallback(drawMaterialSet, single.material.get(), single.materialInstance.get());
                const int useAlbedoTexture = ComputeForwardAlbedoTextureFlag(single.material.get(), single.materialInstance.get());
                if (!useTypedForwardPush)
                    sh->SetInt("u_UseAlbedoTexture", useAlbedoTexture);
                const math::Mat4* bonePtr = single.bones;
                size_t boneCount = single.boneCount;
                auto collectBones = [&](const std::string& name, const math::Mat4* values, size_t count) {
                    if (name == "u_Bones" && values && count > 0) { bonePtr = values; boneCount = count; }
                };
                if ((!bonePtr || boneCount == 0) && single.materialInstance) single.materialInstance->ForEachMat4Array(collectBones);
                else if (!bonePtr || boneCount == 0) baseMat->ForEachMat4Array(collectBones);
                rhi::IDescriptorSet* sceneSetForDraw = sceneSetForCall;
                if (bonePtr && boneCount > 0 && m_forwardBonesBuffer) {
                    boneCount = std::min(boneCount, kMaxBones);
                    const uint32_t boneBytes = static_cast<uint32_t>(boneCount * sizeof(math::Mat4));
                    m_forwardBonesBuffer->Update(bonePtr, 0, boneBytes);
                    rhi::IDescriptorSet* drawSceneSet = AcquireForwardDrawSceneBuffersSet();
                    if (drawSceneSet) {
                        drawSceneSet->UpdateStorageBuffer(0, m_sceneLightingBuffer, 0, 0);
                        drawSceneSet->UpdateStorageBuffer(1, m_forwardBonesBuffer, 0, boneBytes);
                        drawSceneSet->UpdateStorageBuffer(2, m_sceneLightingBuffer, 0, 0);
                        rhi::IBuffer* clusterOrDummy = m_lightIndexBuffer ? m_lightIndexBuffer : m_sceneLightingBuffer;
                        drawSceneSet->UpdateStorageBuffer(3, clusterOrDummy, 0, 0);
                        sceneSetForDraw = drawSceneSet;
                    }
                }
                rhi::IDescriptorSet* sets[5] = { m_forwardFrameSet, passSetForCall, drawMaterialSet, bindlessSet, sceneSetForDraw };
                cmd->BindDescriptorSets(0, 5, sets);
                if (useTypedForwardPush) {
                    abi::WriteForwardPushConstants(cmd, single.transform, useIBL, useAlbedoTexture);
                } else {
                    sh->SetMat4("u_Model", single.transform);
                    sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(single.transform)));
                }
                debug::Profiler::AddFrameCounter("drawCalls", 1);
                debug::Profiler::AddFrameCounter("triangles", GetIndexCountForDraw(single.mesh.get(), single.submeshIndex) / 3u);
                single.mesh->Bind(m_device);
                { MeshHandle h = single.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device, single.submeshIndex); }
                single.mesh->Unbind();
                sh->Unbind();
            }
        }
    }
}

void RenderPipeline::RenderForwardPlus(const IRenderSceneView& view) {
    if (!m_clusteredLightGrid) {
        RenderForward(view);
        return;
    }
    BuildAndExecuteForwardPlusFrameGraph(view);
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

    const CameraView cam = m_currentFrameData ? m_currentFrameData->camera : AdjustCameraForBackend(m_device, view.GetCamera());
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
            RenderForward(view);
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
    PostProcessPassParams postParams = BuildPostProcessPassParams(&view, cam);
    ExecutePostProcessPass(cmd, postParams);

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

void RenderPipeline::EnsureSkyboxShader() {
    if (m_skyboxShader && m_skyboxShader->IsValid()) return;
    if (!m_device) return;
    if (m_usingShared && m_usingShared->skyboxShader && m_usingShared->skyboxShader->IsValid()) {
        m_skyboxShader = m_usingShared->skyboxShader;
        m_cachedSkyboxTexture = nullptr;
        return;
    }
    m_skyboxShader = std::make_shared<Shader>();
    rhi::DepthStencilState skyboxDepth;
    skyboxDepth.depthTest = true;
    skyboxDepth.depthWrite = true;
    skyboxDepth.depthCompare = rhi::DepthCompare::LessEqual;
    if (!m_skyboxShader->Load(m_device, "passes/sky/skybox", Mesh::GetFullscreenQuadVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList, &skyboxDepth, rhi::PipelineLayoutKind::Post,
                              std::optional{rhi::StandardAttachmentFormats::PostHDRWithDepth()})) {
        core::Log::WarnF("Skybox shader failed to load (passes/sky/skybox). Run from project root or ensure assets/shaders/opengl/ is available.");
        return;
    }
    if (!m_skyboxShader->IsValid()) {
        core::Log::Warn("Skybox shader invalid after load.");
        return;
    }
    if (m_sharedResources) m_sharedResources->skyboxShader = m_skyboxShader;
    m_cachedSkyboxTexture = nullptr;
}

void RenderPipeline::EnsureProceduralSkyboxShader() {
    if (m_proceduralSkyboxShader && m_proceduralSkyboxShader->IsValid()) return;
    if (!m_device) return;
    if (m_usingShared && m_usingShared->proceduralSkyboxShader && m_usingShared->proceduralSkyboxShader->IsValid()) {
        m_proceduralSkyboxShader = m_usingShared->proceduralSkyboxShader;
        return;
    }
    m_proceduralSkyboxShader = std::make_shared<Shader>();
    rhi::DepthStencilState skyboxDepth;
    skyboxDepth.depthTest = true;  // TEST: disable to isolate depth
    skyboxDepth.depthWrite = true;
    skyboxDepth.depthCompare = rhi::DepthCompare::LessEqual;
    if (!m_proceduralSkyboxShader->Load(m_device, "passes/sky/skybox_procedural", Mesh::GetFullscreenQuadVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList, &skyboxDepth, rhi::PipelineLayoutKind::Post,
                                        std::optional{rhi::StandardAttachmentFormats::PostHDRWithDepth()})) {
        core::Log::WarnF("Procedural skybox shader failed to load (passes/sky/skybox_procedural).");
        return;
    }
    if (!m_proceduralSkyboxShader->IsValid()) {
        core::Log::Warn("Procedural skybox shader invalid after load.");
        return;
    }
    if (m_sharedResources) m_sharedResources->proceduralSkyboxShader = m_proceduralSkyboxShader;
}

static constexpr uint32_t kPostPassBindingCount = 7;

void RenderPipeline::EnsureSkyboxDummyTexture() {
    if (m_skyboxDummyTexture || !m_device) return;
    rhi::TextureDesc desc = {};
    desc.type = rhi::TextureType::Texture2D;
    desc.width = 1;
    desc.height = 1;
    desc.format = rhi::RHITextureFormat::RGBA8;
    desc.usage = rhi::TextureUsage::ShaderResource;
    desc.mipLevels = 1;
    static const uint8_t black[4] = { 0, 0, 0, 255 };
    m_skyboxDummyTexture = m_device->CreateTexture(desc, black);
}

void RenderPipeline::EnsureSkyboxPostSetFilled(rhi::ITexture* binding0Tex) {
    if (!m_device)
        return;
    const uint32_t frameIndex = m_device->GetCurrentFrameIndex() % kSkyboxPostSetsFrames;
    const uint32_t frameNumber = m_device->GetFrameNumber();
    rhi::IDescriptorSet*& setRef = m_skyboxPostSets[frameIndex];
    if (!setRef) {
        rhi::IPipelineLayout* postLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::PostPass);
        if (!postLayout)
            return;
        setRef = m_device->CreateDescriptorSet(postLayout);
        if (!setRef)
            return;
    }
    rhi::ITexture* expectedTex = binding0Tex ? binding0Tex : m_skyboxDummyTexture;
    if (m_lastSkyboxPostSetFilledFrameNumber == frameNumber && m_cachedSkyboxTexture == expectedTex)
        return;
    EnsureSkyboxDummyTexture();
    rhi::ITexture* dummy = m_skyboxDummyTexture;
    if (!dummy)
        return;
    rhi::ITexture* tex0 = binding0Tex ? binding0Tex : dummy;
    setRef->UpdateTexture(0, tex0);
    for (uint32_t b = 1; b < kPostPassBindingCount; ++b)
        setRef->UpdateTexture(b, dummy);
    m_lastSkyboxPostSetFilledFrameNumber = frameNumber;
    m_cachedSkyboxTexture = tex0;
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
    m_frameGraph->AddPass("Skybox",
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
    const uint32_t vpW = rt ? rt->GetWidth() : m_width;
    const uint32_t vpH = rt ? rt->GetHeight() : m_height;
    rhi::ICommandList* cmdSky = GetRHICommandList();
    rhi::ITexture* skyboxTex = skybox->GetTextureRHI();
    if (cmdSky && rt) {
        cmdSky->SetRenderTarget(rt);
        cmdSky->SetViewport(0, 0, vpW, vpH);
    }
    if (rt == m_hdrRenderTarget && useGbufferDepth && m_gbuffer && m_device && m_hdrRenderTarget)
        m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_hdrRenderTarget);
    if (cmdSky && rt)
        cmdSky->SetRenderTarget(rt);
    // Keep prior passes (forward/transparency) intact; skybox should fill only background pixels.

    if (cmdSky) {
        cmdSky->SetCullFace(false);
        cmdSky->SetDepthTest(true);
        cmdSky->SetDepthWrite(true);
        // Skybox draws at far depth; use <= so clear-depth background (1.0) is still drawable.
        cmdSky->SetDepthFuncLess(false);
    }
    m_skyboxShader->Bind(m_device);
    if (m_device && m_device->GetRenderBackend() == RenderBackend::Vulkan && cmdSky) {
        abi::WriteSkyboxInversePushConstants(cmdSky, glm::inverse(camera.view), glm::inverse(camera.projection));
    } else {
        m_skyboxShader->SetMat4("u_InverseViewMatrix", glm::inverse(camera.view));
        m_skyboxShader->SetMat4("u_InverseProjectionMatrix", glm::inverse(camera.projection));
    }
    m_skyboxShader->SetInt("u_Skybox", 0);
    EnsureForwardResourceSets();
    EnsureSkyboxPostSetFilled(skyboxTex);
    const uint32_t frameIndex = m_device ? m_device->GetCurrentFrameIndex() % kSkyboxPostSetsFrames : 0u;
    rhi::IDescriptorSet* skyboxPostSet = m_skyboxPostSets[frameIndex];
    if (!m_forwardFrameSet || !skyboxPostSet) {
        core::Log::InfoF("Skybox cubemap: skip draw (forwardFrameSet=%p skyboxPostSet=%p frameIndex=%u)", (void*)m_forwardFrameSet, (void*)skyboxPostSet, frameIndex);
        return;
    }
    rhi::IPipeline* pipe = m_skyboxShader->GetPipelineRHI();
    if (!cmdSky || !pipe) {
        core::Log::InfoF("Skybox cubemap: skip draw (cmdSky=%p pipe=%p)", (void*)cmdSky, (void*)pipe);
        return;
    }
    cmdSky->BindPipeline(pipe);
    // Sanity check: set to true to skip binding set 0; validation should report missing/wrong set and confirm set 0 is in the path.
    static const bool kSkyboxSanityCheckSkipSet0 = false;
    if (kSkyboxSanityCheckSkipSet0) {
        cmdSky->BindDescriptorSets(1, 1, &skyboxPostSet);  // only set 1 (PostPass); set 0 (Frame) not bound
    } else {
        rhi::IDescriptorSet* sets[2] = { m_forwardFrameSet, skyboxPostSet };
        cmdSky->BindDescriptorSets(0, 2, sets);
    }
    if (vpW == 0 || vpH == 0) {
        core::Log::WarnF("Skybox cubemap: zero viewport vpW=%u vpH=%u (draw skipped)", vpW, vpH);
        return;
    }
    cmdSky->SetViewport(0, 0, vpW, vpH);
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

void RenderPipeline::RenderProceduralSkyboxImpl(const CameraView& camera, bool useGbufferDepth, rhi::IRenderTarget* targetOverride) {
    EnsureProceduralSkyboxShader();
    if (!m_proceduralSkyboxShader || !m_proceduralSkyboxShader->IsValid()) return;

    rhi::IRenderTarget* rt = targetOverride ? targetOverride : m_hdrRenderTarget;
    const uint32_t vpW = rt ? rt->GetWidth() : m_width;
    const uint32_t vpH = rt ? rt->GetHeight() : m_height;
    rhi::ICommandList* cmdSky = GetRHICommandList();
    if (cmdSky && rt) {
        cmdSky->SetRenderTarget(rt);
        cmdSky->SetViewport(0, 0, vpW, vpH);
    }
    if (rt == m_hdrRenderTarget && useGbufferDepth && m_gbuffer && m_device && m_hdrRenderTarget)
        m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_hdrRenderTarget);
    if (cmdSky && rt)
        cmdSky->SetRenderTarget(rt);

    if (cmdSky)
        cmdSky->SetCullFace(false);
    if (cmdSky) {
        cmdSky->SetDepthTest(true);
        cmdSky->SetDepthWrite(true);
        // Same as cubemap skybox path: allow depth == 1.0 background.
        cmdSky->SetDepthFuncLess(false);
    }

    m_proceduralSkyboxShader->Bind(m_device);
    if (m_device && m_device->GetRenderBackend() == RenderBackend::Vulkan && cmdSky) {
        abi::WriteSkyboxInversePushConstants(cmdSky, glm::inverse(camera.view), glm::inverse(camera.projection));
    } else {
        m_proceduralSkyboxShader->SetMat4("u_InverseViewMatrix", glm::inverse(camera.view));
        m_proceduralSkyboxShader->SetMat4("u_InverseProjectionMatrix", glm::inverse(camera.projection));
    }

    EnsureForwardResourceSets();
    EnsureSkyboxPostSetFilled(nullptr);
    const uint32_t frameIndex = m_device ? m_device->GetCurrentFrameIndex() % kSkyboxPostSetsFrames : 0u;
    rhi::IDescriptorSet* skyboxPostSet = m_skyboxPostSets[frameIndex];
    if (!m_forwardFrameSet || !skyboxPostSet) {
        core::Log::InfoF("Skybox procedural: skip draw (forwardFrameSet=%p skyboxPostSet=%p frameIndex=%u)", (void*)m_forwardFrameSet, (void*)skyboxPostSet, frameIndex);
        return;
    }

    rhi::IPipeline* pipe = m_proceduralSkyboxShader->GetPipelineRHI();
    if (!cmdSky || !pipe) {
        core::Log::InfoF("Skybox procedural: skip draw (cmdSky=%p pipe=%p)", (void*)cmdSky, (void*)pipe);
        return;
    }
    cmdSky->BindPipeline(pipe);
    rhi::IDescriptorSet* sets[2] = { m_forwardFrameSet, skyboxPostSet };
    cmdSky->BindDescriptorSets(0, 2, sets);
    if (vpW == 0 || vpH == 0) {
        core::Log::WarnF("Skybox procedural: zero viewport vpW=%u vpH=%u (draw skipped)", vpW, vpH);
        return;
    }
    cmdSky->SetViewport(0, 0, vpW, vpH);
    cmdSky->DrawFullscreenQuad(pipe);

    m_proceduralSkyboxShader->Unbind();
    if (cmdSky) {
        cmdSky->SetDepthTest(true);
        cmdSky->SetDepthWrite(true);
        cmdSky->SetDepthFuncLess(true);
        cmdSky->SetCullFace(true);
        cmdSky->SetRenderTarget(nullptr);
    }
}

void RenderPipeline::RenderSkybox(const IRenderSceneView& view, bool useGbufferDepth, rhi::IRenderTarget* targetOverride) {
    const CameraView cam = AdjustCameraForBackend(m_device, view.GetCamera());
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
    RenderForwardOpaque(cmd, captureFrame);
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
    if (!m_device->GetCapabilities().supportsCompute) return;
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
    if (!m_streamingNV12Pipeline || !sourceRT || !nv12BufferHandle || nv12Size == 0 || pitch == 0) return;
    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;
    rhi::ITexture* tex = sourceRT->GetColorTexture(0);
    if (!tex) return;
    cmd->BindPipeline(m_streamingNV12Pipeline);
    m_device->EnsureComputePipelineStorageBlockBindings(m_streamingNV12Pipeline);
    cmd->BindTexture(tex, 0);
    cmd->BindPixelPackBufferAsSSBO(nv12BufferHandle, 0, nv12Size);
    if (m_device->GetRenderBackend() == RenderBackend::Vulkan) {
        abi::WriteRgbaToNv12PushConstants(cmd, width, height, pitch);
    } else {
        cmd->SetUniformUint("u_Width", width);
        cmd->SetUniformUint("u_Height", height);
        cmd->SetUniformUint("u_Pitch", pitch);
    }
    const uint32_t gx = (width + 15u) / 16u;
    const uint32_t gy = (height + 15u) / 16u;
    cmd->Dispatch(gx > 0 ? gx : 1u, gy > 0 ? gy : 1u, 1u);
    cmd->MemoryBarrierStorage();
}

void RenderPipeline::RenderEntityIDPass(const IRenderSceneView& view, const std::vector<uint32_t>& entityIdsForOpaque) {
    if (!m_entityIdRenderTarget || entityIdsForOpaque.empty()) return;
    EnsureEntityIdShader();
    if (!m_entityIdShader || !m_entityIdShader->IsValid()) return;

    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;

    const CameraView cam = m_currentFrameData ? m_currentFrameData->camera : AdjustCameraForBackend(m_device, view.GetCamera());
    const bool useTypedEntityIdPush = (m_device && m_device->GetRenderBackend() == ::eyelib::render::RenderBackend::Vulkan);
    cmd->SetRenderTarget(m_entityIdRenderTarget);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(true);

    rhi::ClearValue clearId = {};
    clearId.colorUint = 0u;
    cmd->ClearRenderTarget(m_entityIdRenderTarget, clearId);

    m_entityIdShader->Bind(m_device);
    if (useTypedEntityIdPush) {
        abi::WriteEntityIdViewProjectionPushConstants(cmd, cam.view, cam.projection);
    } else {
        m_entityIdShader->SetMat4("u_View", cam.view);
        m_entityIdShader->SetMat4("u_Projection", cam.projection);
    }

    size_t index = 0;
    view.ForEachOpaqueDrawable([&](const Drawable& d) {
        if (!d.mesh || index >= entityIdsForOpaque.size()) { ++index; return; }
        uint32_t id = entityIdsForOpaque[index++];
        if (useTypedEntityIdPush) {
            abi::WriteEntityIdFragPushConstants(cmd, id);
            abi::WriteEntityIdModelPushConstants(cmd, d.transform);
        } else {
            m_entityIdShader->SetUint("u_EntityID", id);
            m_entityIdShader->SetMat4("u_Model", d.transform);
        }
        d.mesh->Bind(m_device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device, d.submeshIndex); }
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

    rhi::ITexture* sceneTex = m_outlineCompositeTemp->GetColorTexture(0);
    rhi::ITexture* idTex = m_entityIdRenderTarget->GetColorTexture(0);
    if (sceneTex) rhi::BarrierTextureForShaderRead(cmd, sceneTex);
    if (idTex) rhi::BarrierTextureForShaderRead(cmd, idTex);

    cmd->SetRenderTarget(m_hdrRenderTarget);
    cmd->SetViewport(0, 0, m_width, m_height);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);

    m_outlinePostShader->Bind(m_device);
    rhi::IDescriptorSet* outlineSet = m_outlinePostSets[m_device->GetCurrentFrameIndex() % kOutlinePostSetsFrames];
    if (outlineSet && sceneTex && idTex) {
        EnsureOutlinePostSetFilled(sceneTex, idTex);
        cmd->BindDescriptorSets(0, 1, &outlineSet);
    } else {
        if (sceneTex) cmd->BindTexture(sceneTex, 0);
        if (idTex) cmd->BindTexture(idTex, 1);
    }
    const float invW = m_width > 0 ? 1.0f / static_cast<float>(m_width) : 0.0f;
    const float invH = m_height > 0 ? 1.0f / static_cast<float>(m_height) : 0.0f;
    const bool useTypedOutlinePost = (m_device && m_device->GetRenderBackend() == ::eyelib::render::RenderBackend::Vulkan);
    if (useTypedOutlinePost) {
        abi::WriteOutlinePostPushConstants(cmd, selectedPackedId, math::Vec3(r, g, b), math::Vec2(invW, invH));
    } else {
        m_outlinePostShader->SetUint("u_SelectedID", selectedPackedId);
        m_outlinePostShader->SetVec3("u_OutlineColor", math::Vec3(r, g, b));
        m_outlinePostShader->SetVec2("u_InvSize", math::Vec2(invW, invH));
    }

    rhi::IPipeline* pipe = m_outlinePostShader->GetPipelineRHI();
    if (pipe) cmd->DrawFullscreenQuad(pipe);

    m_outlinePostShader->Unbind();
    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(true);
    cmd->SetCullFace(true);
    cmd->SetRenderTarget(nullptr);
    if (rhi::ITexture* outTex = m_hdrRenderTarget->GetColorTexture(0))
        rhi::BarrierTextureForShaderRead(cmd, outTex);
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
    if (m_shaderCache) m_shaderCache->reloadAll();
    if (m_skyboxShader) m_skyboxShader->Reload();
    if (m_proceduralSkyboxShader) m_proceduralSkyboxShader->Reload();
    if (m_outlinePostShader) m_outlinePostShader->Reload();
    if (m_entityIdShader) m_entityIdShader->Reload();
    if (m_stereoForwardShader) m_stereoForwardShader->Reload();
    if (m_gbufferPass) m_gbufferPass->ReloadShaders();
    if (m_shadowPass) m_shadowPass->ReloadShaders();
    if (m_lightingPass) m_lightingPass->ReloadShaders();
    if (m_postProcessor) m_postProcessor->ReloadShaders();
}

GBuffer* RenderPipeline::GetGBuffer() { return m_gbuffer.get(); }
CascadeShadowMap* RenderPipeline::GetCascadeShadowMap() { return m_cascadeShadowMap.get(); }
PostProcessor* RenderPipeline::GetPostProcessor() { return m_postProcessor.get(); }

const std::vector<FrameGraph::PassInfo>& RenderPipeline::GetFrameGraphPasses() const {
    static const std::vector<FrameGraph::PassInfo> empty;
    return m_frameGraph ? m_frameGraph->GetPasses() : empty;
}

} // namespace render
} // namespace eyelib
