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
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/render/ShaderSourceLoader.h"

#include "eyelib/math/TransformUtils.h"
#include "eyelib/math/Frustum.h"
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
    m_shadowPass.reset();
    m_gpuDrivenQuad.reset();
    m_lightCullPass.reset();
    m_gpuCullPass.reset();
    m_gbufferPass.reset();
    m_shaderCache.reset();
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

    m_shaderCache = std::make_unique<ShaderCache>();
    m_gbufferPass = std::make_unique<GBufferPass>();
    m_gbufferPass->SetShaderCache(m_shaderCache.get());
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
        m_shadowPass->SetShaderCache(m_shaderCache.get());
    m_lightingPass = std::make_unique<LightingPass>();

    // Create pass default shaders once at init (no lazy creation during render).
    // GBuffer shaders come from ShaderCache + MaterialCompiler only; no standalone load.
    {
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

    m_postProcessor = std::make_unique<PostProcessor>();
    if (!m_postProcessor->Initialize(width, height, m_device)) {
        core::Log::Error("RenderPipeline::Initialize: PostProcessor::Initialize failed");
        return false;
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
} // namespace

void RenderPipeline::PrepareFrameData(const IRenderSceneView& view) {
    m_frameData.camera = view.GetCamera();
    m_frameData.light = view.GetLight();
    m_frameData.skybox = view.GetSkybox();
    m_frameData.pointLights.clear();
    m_frameData.spotLights.clear();
    view.ForEachPointLight([this](const PointLight& p) { m_frameData.pointLights.push_back(p); });
    view.ForEachSpotLight([this](const SpotLight& s) { m_frameData.spotLights.push_back(s); });

    math::Frustum cameraFrustum;
    cameraFrustum.FromMatrix(m_frameData.camera.projection * m_frameData.camera.view);

    m_frameData.allOpaque.clear();
    m_frameData.visibleOpaque.clear();
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
        if (m_ibl->Initialize(skyTex, 32, 512)) m_iblEnvironmentTexture = skyTex;
    }
    return m_ibl->IsValid() ? m_ibl.get() : nullptr;
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
    const float aspect = (camView.projection[0][0] != 0.0f) ? (camView.projection[1][1] / camView.projection[0][0]) : 1.0f;
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
                RunShadowPassBatched(c.cmd, *m_currentFrameData, m_currentFrameData->camera);
            };
        if (name == "GBuffer")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                RenderOpaqueGeometryToGBuffer(c.cmd, *m_currentFrameData, m_currentFrameData->camera, m_lastGpuCullDrawListCount);
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
                    const CameraView& camView = m_currentFrameData->camera;
                    const LightParams& lightParams = m_currentFrameData->light;
                    const rhi::ClearValue clearBlack = { 0.f, 0.f, 0.f, 1.f };
                    if (c.cmd && m_hdrRenderTarget)
                        rhi::ClearAndSetRenderTargetWithViewport(c.cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearBlack);
                    CascadeShadowMap* cascadeForLighting = lightParams.castShadows ? m_cascadeShadowMap.get() : nullptr;
                    math::Mat4 lightSpaceMatrix(1.0f);
                    if (cascadeForLighting && cascadeForLighting->GetSplitCount() > 0)
                        lightSpaceMatrix = cascadeForLighting->GetLightSpaceMatrix(0);
                    FillSceneLightingBuffer(m_sceneLightingBuffer, lightParams, camView.viewPos, lightSpaceMatrix, 0.005f,
                        lightParams.castShadows, &m_currentFrameData->pointLights, &m_currentFrameData->spotLights, cascadeForLighting);
                    IBL* iblPtr = EnsureIBLFromFrameData(*m_currentFrameData);
                    m_lightingPass->Render(*m_gbuffer, c, lightParams, camView.view, camView.projection, camView.viewPos, cascadeForLighting,
                        &m_currentFrameData->pointLights, &m_currentFrameData->spotLights, m_sceneLightingBuffer, iblPtr);
                    debug::Profiler::AddFrameCounter("drawCalls", 1);
                    debug::Profiler::AddFrameCounter("triangles", 2);
                    debug::Profiler::AddFrameCounter("stateChanges", 1);
                };
            if (name == "Transparency")
                return [this](const FGPassResources&) {
                    if (!m_currentFrameData) return;
                    RenderContext c = RenderContext::FromDevice(m_device);
                    const CameraView& camView = m_currentFrameData->camera;
                    if (m_device && m_hdrRenderTarget)
                        m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_hdrRenderTarget);
                    if (c.cmd && m_hdrRenderTarget) {
                        c.cmd->SetRenderTarget(m_hdrRenderTarget);
                        c.cmd->SetViewport(0, 0, m_width, m_height);
                    }
                    TransparencyPass::Render(c, m_currentFrameData->visibleTransparent, camView.view, camView.projection, camView.viewPos, m_shaderCache.get());
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
    m_lastGpuCullDrawListCount = PrepareGpuCullFromFrameData(m_frameData, m_frameData.camera);
    m_currentFrameData = &m_frameData;

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
            return [this](const FGPassResources&) { RenderContext c = RenderContext::FromDevice(m_device); if (m_gpuCullPass) m_gpuCullPass->Execute(c); };
        if (name == "Shadow")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                RunShadowPassBatched(c.cmd, *m_currentFrameData, m_currentFrameData->camera);
            };
        if (name == "Forward Opaque")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                const rhi::ClearValue clearFwd = { 0.1f, 0.1f, 0.15f, 1.f };
                if (c.cmd && m_hdrRenderTarget)
                    rhi::ClearAndSetRenderTargetWithViewport(c.cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearFwd);
                if (c.cmd) { c.cmd->SetDepthTest(true); c.cmd->SetCullFace(false); }
                RenderForwardOpaque(c.cmd, *m_currentFrameData);
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
                const CameraView& camView = m_currentFrameData->camera;
                if (c.cmd && m_hdrRenderTarget) {
                    c.cmd->SetRenderTarget(m_hdrRenderTarget);
                    c.cmd->SetViewport(0, 0, m_width, m_height);
                }
                TransparencyPass::Render(c, m_currentFrameData->visibleTransparent, camView.view, camView.projection, camView.viewPos, m_shaderCache.get());
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
                RunShadowPassBatched(c.cmd, *m_currentFrameData, m_currentFrameData->camera);
            };
        if (name == "Forward+ Opaque")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                PrepareForwardPlusFrame(c.cmd, *m_currentFrameData);
                RenderForwardPlusOpaqueBatched(c.cmd, *m_currentFrameData);
            };
        if (name == "Transparency")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                const CameraView& camView = m_currentFrameData->camera;
                if (c.cmd && m_hdrRenderTarget) {
                    c.cmd->SetRenderTarget(m_hdrRenderTarget);
                    c.cmd->SetViewport(0, 0, m_width, m_height);
                }
                TransparencyPass::Render(c, m_currentFrameData->visibleTransparent, camView.view, camView.projection, camView.viewPos, m_shaderCache.get());
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
    m_lastGpuCullDrawListCount = PrepareGpuCullFromFrameData(m_frameData, m_frameData.camera);
    m_currentFrameData = &m_frameData;

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
    m_currentFrameData = &m_frameData;

    CompileFrameGraphIfNeeded(PipelineMode::ForwardPlus, false);
    m_frameGraph->Execute(ctx);
}

void RenderPipeline::RenderForwardOpaque(rhi::ICommandList* cmd, const RenderFrameData& frameData) {
    const CameraView& cam = frameData.camera;
    const LightParams& light = frameData.light;
    IBL* iblPtr = EnsureIBLFromFrameData(frameData);
    const ReflectionProbe* envProbe = m_reflectionProbeManager ? m_reflectionProbeManager->FindClosestProbe(cam.viewPos) : nullptr;
    if (!envProbe && m_reflectionProbeManager) envProbe = m_reflectionProbeManager->GetGlobalProbe();
    // Forward shaders (forward.frag, forward_skinned.frag) use scene lighting SSBO at 5, shadow at 6, IBL at 11/12/13
    const int kShadowMapUnit = 6;
    const int kAlbedoTextureUnit = 0;
    const int kNormalTextureUnit = 1;
    const int kMetallicRoughnessTextureUnit = 2;
    const int kMetallicTextureUnit = 3;
    const int kRoughnessTextureUnit = 4;
    const int kAOTextureUnit = 7;   // 5=SSBO, 6=shadow
    const int kEmissiveTextureUnit = 8;
    const bool useShadows = (m_cascadeShadowMap != nullptr && light.castShadows);
    if (useShadows && cmd) {
        rhi::ITexture* shadowTex = m_cascadeShadowMap->GetDepthTextureRHI();
        if (shadowTex) cmd->BindTexture(shadowTex, kShadowMapUnit);
    }
    math::Mat4 lightSpaceMatrix(1.0f);
    if (light.castShadows && m_cascadeShadowMap && m_cascadeShadowMap->GetSplitCount() > 0)
        lightSpaceMatrix = m_cascadeShadowMap->GetLightSpaceMatrix(0);
    FillSceneLightingBuffer(m_sceneLightingBuffer, light, cam.viewPos, lightSpaceMatrix, 0.005f,
        light.castShadows, &frameData.pointLights, &frameData.spotLights, m_cascadeShadowMap.get());
    if (cmd && m_sceneLightingBuffer)
        cmd->BindStorageBuffer(m_sceneLightingBuffer, 5, 0, 0);

    if (!m_shaderCache) { if (useShadows && cmd) cmd->UnbindTexture(kShadowMapUnit); return; }
    std::map<ShaderKey, std::vector<Drawable>> buckets;
    for (const Drawable& d : frameData.visibleOpaque) {
        if (!d.mesh || !d.material) continue;
        const Material* baseMat = d.materialInstance ? d.materialInstance->GetMaterial().get() : d.material.get();
        InstanceType it = (d.bones && d.boneCount > 0) ? InstanceType::Skinned : InstanceType::Static;
        ShaderKey key{ PipelineStage::Forward, it, baseMat->GetMaterialShaderId() };
        buckets[key].push_back(d);
    }
    for (auto& [key, drawables] : buckets) {
        const Material* matForCompile = drawables[0].materialInstance ? drawables[0].materialInstance->GetMaterial().get() : drawables[0].material.get();
        auto shader = m_shaderCache->getOrCompile(m_device, key, matForCompile);
        if (!shader || !shader->IsValid()) continue;
        Shader* sh = shader.get();
        sh->Bind(m_device);
        debug::Profiler::AddFrameCounter("stateChanges", 1);
        sh->SetMat4("u_View", cam.view);
        sh->SetMat4("u_Projection", cam.projection);
        if (cmd) {
            if (envProbe && envProbe->IsValid())
                envProbe->BindForLighting(cmd, 11, 12, 13, iblPtr && iblPtr->IsValid() ? iblPtr->GetBRDFLUTTextureRHI() : nullptr);
            else if (iblPtr && iblPtr->IsValid())
                iblPtr->BindForLighting(cmd, 11, 12, 13);
            sh->SetInt("u_UseIBL", (envProbe && envProbe->IsValid()) || (iblPtr && iblPtr->IsValid()) ? 1 : 0);
        } else {
            sh->SetInt("u_UseIBL", 0);
        }
        for (const Drawable& d : drawables) {
            if (d.materialInstance)
                d.materialInstance->ApplyToShader(m_device, sh, cmd);
            else
                d.material->ApplyToShader(m_device, sh, cmd);
            sh->SetMat4("u_Model", d.transform);
            sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
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
            debug::Profiler::AddFrameCounter("drawCalls", 1);
            debug::Profiler::AddFrameCounter("triangles", GetIndexCountForDraw(d.mesh.get(), d.submeshIndex) / 3u);
            d.mesh->Bind(m_device);
            { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device, d.submeshIndex); }
            d.mesh->Unbind();
        }
        sh->Unbind();
    }

    if (useShadows && cmd) cmd->UnbindTexture(kShadowMapUnit);
}

void RenderPipeline::RenderForwardOpaqueStereo(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& leftCam, const CameraView& rightCam) {
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
                RunShadowPassBatched(c.cmd, *m_currentFrameData, m_currentLeftCam);
            };
        if (name == "Forward Opaque")
            return [this](const FGPassResources&) {
                if (!m_currentFrameData) return;
                RenderContext c = RenderContext::FromDevice(m_device);
                const rhi::ClearValue clearFwd = { 0.1f, 0.1f, 0.15f, 1.f };
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
                TransparencyPass::Render(c, m_currentFrameData->visibleTransparent, m_currentLeftCam.view, m_currentLeftCam.projection, m_currentLeftCam.viewPos, m_shaderCache.get());
                if (c.cmd && m_hdrRenderTarget) {
                    const uint32_t halfW = m_width / 2;
                    c.cmd->SetViewport(halfW, 0, halfW, m_height);
                }
                TransparencyPass::Render(c, m_currentFrameData->visibleTransparent, m_currentRightCam.view, m_currentRightCam.projection, m_currentRightCam.viewPos, m_shaderCache.get());
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
    m_currentLeftCam = leftCam;
    m_currentRightCam = rightCam;
    PrepareFrameData(view);
    m_currentFrameData = &m_frameData;

    RenderContext ctx = RenderContext::FromDevice(m_device);

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
    cmd->End();
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
    TransparencyPass::Render(ctx, m_frameData.visibleTransparent, cam.view, cam.projection, cam.viewPos, m_shaderCache.get());
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
    if (cmd && m_lightIndexBuffer) {
        cmd->BindStorageBuffer(m_lightIndexBuffer, 2);
    }

    if (cmd && m_hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, cv);
    }
    if (cmd) { cmd->SetDepthTest(true); cmd->SetCullFace(false); }

    const int kShadowMapUnit = static_cast<int>(rhi::PassBindings::ShadowMap);
    if (light.castShadows && m_cascadeShadowMap && cmd) {
        rhi::ITexture* shadowTex = m_cascadeShadowMap->GetDepthTextureRHI();
        if (shadowTex) cmd->BindTexture(shadowTex, kShadowMapUnit);
    }
    math::Mat4 lightSpaceMatrix(1.0f);
    if (light.castShadows && m_cascadeShadowMap && m_cascadeShadowMap->GetSplitCount() > 0)
        lightSpaceMatrix = m_cascadeShadowMap->GetLightSpaceMatrix(0);
    FillSceneLightingBuffer(m_sceneLightingBuffer, light, cam.viewPos, lightSpaceMatrix, 0.005f,
        light.castShadows, &frameData.pointLights, &frameData.spotLights, m_cascadeShadowMap.get());
    if (cmd && m_sceneLightingBuffer) {
        cmd->BindStorageBuffer(m_sceneLightingBuffer, 5, 0, 0);
    }
}

void RenderPipeline::RenderForwardPlusOpaqueBatched(rhi::ICommandList* cmd, const RenderFrameData& frameData) {
    const CameraView& cam = frameData.camera;
    const LightParams& light = frameData.light;
    const std::vector<PointLight>& pointLights = frameData.pointLights;
    const std::vector<SpotLight>& spotLights = frameData.spotLights;

    if (!m_shaderCache) return;

    const int kShadowMapUnit = static_cast<int>(rhi::PassBindings::ShadowMap);
    auto setCommonForwardPlusUniforms = [&](Shader* sh) {
        sh->SetMat4("u_View", cam.view);
        sh->SetMat4("u_Projection", cam.projection);
        sh->SetInt("u_ScreenWidth", static_cast<int>(m_width));
        sh->SetInt("u_ScreenHeight", static_cast<int>(m_height));
        sh->SetInt("u_TileCountX", static_cast<int>(m_clusteredLightGrid->GetTileCountX()));
        sh->SetInt("u_TileCountY", static_cast<int>(m_clusteredLightGrid->GetTileCountY()));
        sh->SetInt("u_SliceCount", static_cast<int>(m_clusteredLightGrid->GetSliceCount()));
        sh->SetFloat("u_LogNear", m_clusteredLightGrid->GetLogNear());
        sh->SetFloat("u_LogFar", m_clusteredLightGrid->GetLogFar());
        sh->SetFloat("u_NearPlane", m_clusteredLightGrid->GetNearPlane());
        sh->SetFloat("u_FarPlane", m_clusteredLightGrid->GetFarPlane());
        sh->SetInt("u_PointLightCount", static_cast<int>(pointLights.size()));
        sh->SetInt("u_SpotLightCount", static_cast<int>(spotLights.size()));
        sh->SetInt("u_ShadowMap", kShadowMapUnit);
    };

    IBL* iblPtr = EnsureIBLFromFrameData(frameData);
    const ReflectionProbe* envProbe = m_reflectionProbeManager ? m_reflectionProbeManager->FindClosestProbe(cam.viewPos) : nullptr;
    if (!envProbe && m_reflectionProbeManager) envProbe = m_reflectionProbeManager->GetGlobalProbe();
    const int kAlbedoTextureUnit = 0;
    const int kNormalTextureUnit = 1;
    const int kMetallicRoughnessTextureUnit = 2;
    const int kAOTextureUnit = 3;
    const int kEmissiveTextureUnit = 4;
    const math::Vec4 kDefaultUVTransform(0.0f, 0.0f, 1.0f, 1.0f);
    constexpr uint32_t kForwardPlusInstanceBinding = 6u;

    auto drawOneForwardPlus = [&](Shader* sh, const Drawable& d) {
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!d.mesh || !material) return;
        if (d.materialInstance)
            d.materialInstance->ApplyToShader(m_device, sh, cmd);
        else
            d.material->ApplyToShader(m_device, sh, cmd);
        sh->SetMat4("u_Model", d.transform);
        sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
        const math::Mat4* bonePtr = d.bones;
        size_t boneCount = d.boneCount;
        auto collectBones = [&](const std::string& name, const math::Mat4* values, size_t count) {
            if (name == "u_Bones" && values && count > 0) { bonePtr = values; boneCount = count; }
        };
        if ((!bonePtr || boneCount == 0) && d.materialInstance) d.materialInstance->ForEachMat4Array(collectBones);
        else if (!bonePtr || boneCount == 0) material->ForEachMat4Array(collectBones);
        if (bonePtr && boneCount > 0) {
            boneCount = std::min(boneCount, kMaxBones);
            sh->SetMat4Array("u_Bones", bonePtr, boneCount);
        }
        d.mesh->Bind(m_device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(m_device, d.submeshIndex); }
        d.mesh->Unbind();
    };

    std::vector<DrawBatch> batches = BuildBatches(frameData.visibleOpaque);
    bool commonSetInstanced = false;
    bool commonSetNonSkinned = false;
    bool commonSetSkinned = false;

    for (DrawBatch& batch : batches) {
        if (batch.IsInstanced() && m_instanceMatrixBuffer && batch.transforms.size() <= kMaxInstanceBatchSize && batch.material) {
            ShaderKey key{ PipelineStage::ForwardPlus, InstanceType::Instanced, batch.material->GetMaterialShaderId() };
            auto shader = m_shaderCache->getOrCompile(m_device, key, batch.material.get());
            if (shader && shader->IsValid()) {
                Shader* sh = shader.get();
                sh->Bind(m_device);
                debug::Profiler::AddFrameCounter("stateChanges", 1);
                if (!commonSetInstanced) { setCommonForwardPlusUniforms(sh); commonSetInstanced = true; }
                if (cmd) {
                    if (envProbe && envProbe->IsValid()) envProbe->BindForLighting(cmd, 11, 12, 13, iblPtr && iblPtr->IsValid() ? iblPtr->GetBRDFLUTTextureRHI() : nullptr);
                    else if (iblPtr && iblPtr->IsValid()) iblPtr->BindForLighting(cmd, 11, 12, 13);
                    sh->SetInt("u_UseIBL", (envProbe && envProbe->IsValid()) || (iblPtr && iblPtr->IsValid()) ? 1 : 0);
                } else { sh->SetInt("u_UseIBL", 0); }
                if (batch.materialInstance)
                    batch.materialInstance->ApplyToShader(m_device, sh, cmd);
                else
                    batch.material->ApplyToShader(m_device, sh, cmd);
                const size_t byteSize = batch.transforms.size() * sizeof(math::Mat4);
                m_instanceMatrixBuffer->Update(batch.transforms.data(), 0, static_cast<uint32_t>(byteSize));
                if (cmd) {
                    cmd->BindStorageBuffer(m_instanceMatrixBuffer, kForwardPlusInstanceBinding, 0, static_cast<uint32_t>(batch.transforms.size() * 64u));
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
                if (useSkinned) { if (!commonSetSkinned) { setCommonForwardPlusUniforms(sh); commonSetSkinned = true; } } else { if (!commonSetNonSkinned) { setCommonForwardPlusUniforms(sh); commonSetNonSkinned = true; } }
                if (cmd) {
                    if (envProbe && envProbe->IsValid()) envProbe->BindForLighting(cmd, 11, 12, 13, iblPtr && iblPtr->IsValid() ? iblPtr->GetBRDFLUTTextureRHI() : nullptr);
                    else if (iblPtr && iblPtr->IsValid()) iblPtr->BindForLighting(cmd, 11, 12, 13);
                    sh->SetInt("u_UseIBL", (envProbe && envProbe->IsValid()) || (iblPtr && iblPtr->IsValid()) ? 1 : 0);
                } else { sh->SetInt("u_UseIBL", 0); }
                debug::Profiler::AddFrameCounter("drawCalls", 1);
                debug::Profiler::AddFrameCounter("triangles", GetIndexCountForDraw(single.mesh.get(), single.submeshIndex) / 3u);
                drawOneForwardPlus(sh, single);
                sh->Unbind();
            }
        }
    }
    if (light.castShadows && m_cascadeShadowMap && cmd) cmd->UnbindTexture(kShadowMapUnit);
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

    const CameraView cam = view.GetCamera();
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

    // Draw skybox to HDR with depth LEQUAL (behind geometry). When post is used, the frame-graph Skybox pass already filled HDR; we only draw after the graph when output is HDR (no post). Uses cubemap if set, else procedural default.
    if (GetCurrentFrameOutputRT() == m_hdrRenderTarget) {
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
        IBL* ibl = EnsureIBLFromFrameData(m_frameData);
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
        rhi::ITexture* finalTexture = m_postProcessor->ApplyPostProcessing(
            hdrTex, *m_gbuffer, settings,
            cam.view, cam.projection,
            invView, invProj, cam.viewPos, skyboxTex, time, probeTex);
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

void RenderPipeline::EnsureProceduralSkyboxShader() {
    if (m_proceduralSkyboxShader && m_proceduralSkyboxShader->IsValid()) return;
    if (!m_device) return;
    m_proceduralSkyboxShader = std::make_shared<Shader>();
    rhi::DepthStencilState skyboxDepth;
    skyboxDepth.depthTest = true;
    skyboxDepth.depthWrite = false;
    skyboxDepth.depthCompare = rhi::DepthCompare::LessEqual;
    if (!m_proceduralSkyboxShader->Load(m_device, "passes/sky/skybox_procedural", Mesh::GetFullscreenQuadVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList, &skyboxDepth)) {
        core::Log::WarnF("Procedural skybox shader failed to load (passes/sky/skybox_procedural).");
        return;
    }
    if (!m_proceduralSkyboxShader->IsValid()) {
        core::Log::Warn("Procedural skybox shader invalid after load.");
        return;
    }
}

void RenderPipeline::AddSkyboxPassToFrameGraph(FGTextureHandle hdrColor, bool useGbufferDepth) {
    m_frameGraph->AddPass("Skybox",
        [this, useGbufferDepth](const FGPassResources&) {
            if (!m_currentFrameData) return;
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

    m_proceduralSkyboxShader->Bind(m_device);
    m_proceduralSkyboxShader->SetMat4("u_InverseViewMatrix", glm::inverse(camera.view));
    m_proceduralSkyboxShader->SetMat4("u_InverseProjectionMatrix", glm::inverse(camera.projection));

    rhi::IPipeline* pipe = m_proceduralSkyboxShader->GetPipelineRHI();
    if (pipe && cmdSky)
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
    if (view.GetSkybox() && view.GetSkybox()->IsValid())
        RenderSkyboxImpl(view.GetCamera(), view.GetSkybox(), useGbufferDepth, targetOverride);
    else
        RenderProceduralSkyboxImpl(view.GetCamera(), useGbufferDepth, targetOverride);
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
    cmd->SetUniformUint("u_Width", width);
    cmd->SetUniformUint("u_Height", height);
    cmd->SetUniformUint("u_Pitch", pitch);
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
