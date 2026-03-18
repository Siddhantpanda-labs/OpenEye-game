#pragma once

#include "eyelib/render/IRenderSceneView.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/passes/GpuCullPass.h"
#include "eyelib/render/lighting/LightCullPass.h"
#include "eyelib/render/rhi/RHI.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eyelib {
namespace render {

class GBuffer;
class CascadeShadowMap;
class ShaderCache;
class GBufferPass;
class ShadowPass;
class LightingPass;
class ClusteredLightGrid;
class PostProcessor;
class IBL;
class ReflectionProbeManager;
class GpuProfilerRHI;
class Mesh;
class Shader;
class TextureCube;
class FullscreenQuad;

/** Forward: frame-graph (GPU Cull, Shadow, Forward Opaque, Transparency, Skybox, Post). Deferred: frame-graph (GPU Cull, Shadow, GBuffer, Lighting, Transparency, Skybox, Post). ForwardPlus: tiled forward (many lights), skybox drawn after; legacy path. */
enum class PipelineMode { Forward, Deferred, ForwardPlus };

/**
 * Orchestrates render passes via a frame graph for Forward and Deferred.
 * Render(view) dispatches by mode: Deferred/Forward/ForwardPlus each use BuildAndExecute*FrameGraph (single path).
 */
class RenderPipeline {
public:
    using PrePostCallback = std::function<void(rhi::IRenderTarget* hdrRT, unsigned int width, unsigned int height, const RenderScene& scene)>;
    RenderPipeline();
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;

    /** Initialize with owned RHI device (pipeline takes ownership). */
    bool Initialize(std::unique_ptr<rhi::IDevice> device, uint32_t width, uint32_t height);
    /** Initialize with shared RHI device (caller owns device; pipeline does not mutate global state). */
    bool Initialize(rhi::IDevice* sharedDevice, uint32_t width, uint32_t height);
    /** Resize pipeline (GBuffer, HDR RT, post-process). May only be called at frame boundary, before Render() for this frame. Never call mid-frame or while a command list is active. */
    void Resize(uint32_t width, uint32_t height);
    void Shutdown();

    void SetMode(PipelineMode mode) { m_mode = mode; }
    PipelineMode GetMode() const { return m_mode; }

    /** Upload phase: CPU asset updates → staging → vkCmdCopy*. Call after BeginFrame, before Render. Ensures all uploads in one predictable stage. */
    void UpdateUploads();
    /** Render scene (ECS or converted from API at the api layer). */
    void Render(const RenderScene& scene);
    /** Render from a read-only view (zero-copy when view wraps API scene). When entityIdsForOpaque is non-null and size matches opaque drawable count, runs EntityID pass (same depth as main; for editor picking). */
    void Render(const IRenderSceneView& view, const std::vector<uint32_t>* entityIdsForOpaque = nullptr);
    /** Single-pass stereo: one render, two viewports (left | right). Uses instanced draw (2 instances) and stereo shader. Call when pipeline is already resized to full width (width = 2 * eyeWidth). */
    void RenderStereo(const IRenderSceneView& view, const CameraView& leftCam, const CameraView& rightCam);
    /** Draw outline from entity ID buffer (post-process). Call after Render(view, entityIds). selectedPackedId = 0 skips. RGB 0..1. */
    void RenderOutlineFromIdBuffer(uint32_t selectedPackedId, float r = 1.0f, float g = 0.5f, float b = 0.0f);
    /** Draw world-space debug lines with depth test (occluded by geometry). Draws into HDR. */
    void RenderDebugLines(const math::Mat4& view, const math::Mat4& proj,
        const std::vector<std::pair<math::Vec3, math::Vec3>>& lines, float r, float g, float b);
    /** Enqueue debug lines to be drawn before post in the next Render(). Use from scene view so lines are in HDR and get tone-mapped. */
    void SetPendingDebugLines(const math::Mat4& view, const math::Mat4& proj,
        const std::vector<std::pair<math::Vec3, math::Vec3>>& lines, float r, float g, float b);
    /** Enqueue debug lines with per-line color. Drawn before post in the next Render(). */
    void SetPendingDebugLinesColored(const math::Mat4& view, const math::Mat4& proj,
        const std::vector<std::pair<std::pair<math::Vec3, math::Vec3>, math::Vec4>>& linesWithColors);
    void BlitToScreen();
    /** Entity ID render target (R32UI, shared depth with HDR). Null until first use. For ReadPixelUint32 after Render with entity IDs. */
    rhi::IRenderTarget* GetEntityIdRenderTarget() const { return m_entityIdRenderTarget; }
    /** Reload all pipeline shaders that were loaded by name (for hot-reload when shader files change). */
    void ReloadShaders();
    /** Precompile named and default pipelines so Draw() never creates pipelines at runtime. Call once after Initialize. */
    void WarmupPipelines();
    /** Wait for GPU to finish all submitted work (e.g. before swap to avoid flicker). */
    void Finish();
    /** Returns true when the pipeline has a valid render target to present. */
    bool HasValidOutput() const;

    rhi::IDevice* GetRHIDevice() { return m_device; }
    rhi::ICommandList* GetRHICommandList() { return m_device ? m_device->GetImmediateCommandList() : nullptr; }
    /** True when this pipeline owns the device (Initialize(unique_ptr) was used). Caller must run global teardown before Shutdown() when true. */
    bool OwnsDevice() const { return m_ownedDevice != nullptr; }

    GBuffer* GetGBuffer();
    CascadeShadowMap* GetCascadeShadowMap();
    PostProcessor* GetPostProcessor();
    ReflectionProbeManager* GetReflectionProbeManager() { return m_reflectionProbeManager.get(); }
    FrameGraph* GetFrameGraph() { return m_frameGraph.get(); }
    const std::vector<FrameGraph::PassInfo>& GetFrameGraphPasses() const;

    void SetFrameSize(unsigned w, unsigned h);
    void SetPrePostProcessCallback(PrePostCallback cb) { m_prePostCallback = std::move(cb); }

    /** When true and GpuCullPass is supported, deferred GBuffer uses MultiDrawIndexedIndirect (E8). Default false. */
    void SetUseGpuDrivenGBuffer(bool use) { m_useGpuDrivenGBuffer = use; }
    bool GetUseGpuDrivenGBuffer() const { return m_useGpuDrivenGBuffer; }

    /** E9: Bindless heap (null when !supportsBindless). */
    rhi::IBindlessHeap* GetBindlessHeap() const { return m_bindlessHeap; }
    /** E9: Get or allocate bindless handle for texture; returns kInvalidBindlessHandle when bindless unsupported or texture null. */
    rhi::BindlessHandle GetOrAllocateBindlessHandle(rhi::ITexture* texture);

    /** HDR color texture for post-processing (RHI path only; no GL id in this module). */
    rhi::ITexture* GetHDRColorTextureRHI() const;

    /** Render target that will be blitted to screen this frame (post output when valid, else HDR). */
    rhi::IRenderTarget* GetCurrentFrameOutputRT() const;
    /** Read current frame output as RGBA8. Call after Render() and Finish(). outRGBA must be at least width*height*4 bytes. Returns false if readback not supported or RT invalid. */
    bool ReadOutputPixels(uint32_t width, uint32_t height, uint8_t* outRGBA) const;
    /** Color texture of the current frame output (for display; same as BlitToScreen source). */
    rhi::ITexture* GetCurrentFrameOutputColorTextureRHI() const;

    /** Streaming: run compute pass to convert source RT (RGBA) to NV12 in the given buffer. Uses GL_SHADER_STORAGE_BARRIER_BIT only after dispatch. */
    void RunStreamingNV12Pass(rhi::IRenderTarget* sourceRT, void* nv12BufferHandle, size_t nv12Size, uint32_t width, uint32_t height, uint32_t pitch);

    /** Render scene (forward opaque + skybox) to an arbitrary render target. Used for reflection probe cubemap capture. */
    void RenderSceneToTarget(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam, rhi::IRenderTarget* rt);

private:
    /** Shared init after m_device is set (owned or shared). */
    bool InitializeDeviceResources(uint32_t width, uint32_t height);
    void EnsureResources(uint32_t width, uint32_t height);
    void CreateHDRFramebuffer();
    void DestroyHDRFramebuffer();
    /** Frame-graph coordination: compile only when needed (size/mode/stereo change), then execute. */
    void CompileFrameGraphIfNeeded(PipelineMode mode, bool stereo);
    /** Build deferred recipe and add passes (GBuffer, Lighting, etc.). Called from CompileFrameGraphIfNeeded. */
    void BuildDeferredGraph();
    /** Build forward recipe and add passes. Called from CompileFrameGraphIfNeeded. */
    void BuildForwardGraph();
    /** Build forward-stereo recipe and add passes (SkyboxLeft/Right). Called from CompileFrameGraphIfNeeded. */
    void BuildForwardStereoGraph();
    /** Build Forward+ recipe (Shadow, Forward+ Opaque batched, Transparency, Skybox). Called from CompileFrameGraphIfNeeded. */
    void BuildForwardPlusGraph();

    void BuildAndExecuteFrameGraph(const IRenderSceneView& view);
    /** Forward mode: frame graph with GPU Cull, Shadow, Forward Opaque, Transparency, Post. */
    void BuildAndExecuteForwardFrameGraph(const IRenderSceneView& view);
    /** Forward+ mode: frame graph with Shadow, Forward+ Opaque (batched), Transparency, Skybox. */
    void BuildAndExecuteForwardPlusFrameGraph(const IRenderSceneView& view);
    void RenderForward(const IRenderSceneView& view);
    /** Opaque draw loop for forward (lighting + optional shadow). Used by Forward frame graph. */
    void RenderForwardOpaque(rhi::ICommandList* cmd, const RenderFrameData& frameData);
    /** Single-pass stereo opaque: two viewports, stereo shader, DrawInstanced(..., 2). */
    void RenderForwardOpaqueStereo(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& leftCam, const CameraView& rightCam);
    void BuildAndExecuteForwardFrameGraphStereo(const IRenderSceneView& view, const CameraView& leftCam, const CameraView& rightCam);
    void RenderForwardPlus(const IRenderSceneView& view);
    /** Forward+ opaque with batching: instanced draw for multi-instance batches, per-draw for singles/skinned. */
    void RenderForwardPlusOpaqueBatched(rhi::ICommandList* cmd, const RenderFrameData& frameData);
    /** Setup for Forward+ opaque pass: light grid, buffers, clear HDR, bind shadow/scene lighting/light index. */
    void PrepareForwardPlusFrame(rhi::ICommandList* cmd, const RenderFrameData& frameData);
    /** Skybox as a frame-graph pass: writes to HDR so post reads it. Used by Deferred and Forward. */
    /** Adds Skybox pass; executor uses m_currentView (set before Execute). */
    void AddSkyboxPassToFrameGraph(FGTextureHandle hdrColor, bool useGbufferDepth);
    void RenderSkybox(const IRenderSceneView& view, bool useGbufferDepth, rhi::IRenderTarget* targetOverride = nullptr);
    void RenderSkyboxImpl(const CameraView& camera, const std::shared_ptr<TextureCube>& skybox, bool useGbufferDepth, rhi::IRenderTarget* targetOverride = nullptr);
    /** Default when no cubemap: procedural gradient (sky / ground / horizon). Same depth/RT setup as cubemap skybox. */
    void RenderProceduralSkyboxImpl(const CameraView& camera, bool useGbufferDepth, rhi::IRenderTarget* targetOverride = nullptr);
    void EnsureStereoForwardShader();
    void EnsureSkyboxShader();
    void EnsureProceduralSkyboxShader();
    /** Ensures m_skyboxDummyTexture exists (1x1 RGBA8). Used to fill unused Post set bindings. */
    void EnsureSkyboxDummyTexture();
    /** Fills the current frame's skybox Post set: binding 0 = binding0Tex (or dummy if null), bindings 1–6 = dummy. Uses GetCurrentFrameIndex() so we never update a set in use. */
    void EnsureSkyboxPostSetFilled(rhi::ITexture* binding0Tex);
    void EnsureOutlinePostShader();
    void EnsureEntityIdShader();
    void EnsureStreamingNV12Pass();
    void RenderEntityIDPass(const IRenderSceneView& view, const std::vector<uint32_t>& entityIdsForOpaque);
    /** C1: Single opaque-geometry entry point for GBuffer. Chooses CPU submit (per-draw) or GPU submit (indirect) by capability + data. */
    void RenderOpaqueGeometryToGBuffer(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam, uint32_t gpuCullDrawListCount);
    /** Phase D: Run shadow pass with mesh+material batching (instanced + singles). */
    void RunShadowPassBatched(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& camView);

    /** Shared: build draw list from view, upload to GPU, configure GpuCullPass. Returns draw list count. Used by deferred and forward frame graphs. */
    uint32_t PrepareGpuCullFromView(const IRenderSceneView& view, const CameraView& cam);
    /** Fill frame data from view (camera, lights, visible/all opaque, transparent). Call once per frame before graph execute. */
    void PrepareFrameData(const IRenderSceneView& view);
    /** Build GPU cull draw list from frame data visible opaque. Call after PrepareFrameData. */
    uint32_t PrepareGpuCullFromFrameData(const RenderFrameData& frameData, const CameraView& cam);
    /** Ensure IBL is built from frameData.skybox; returns m_ibl.get() when valid, else nullptr. Used by Deferred Lighting, Forward, Forward+. */
    IBL* EnsureIBLFromFrameData(const RenderFrameData& frameData);

    rhi::IDevice* m_device = nullptr;
    std::unique_ptr<rhi::IDevice> m_ownedDevice;
    std::unique_ptr<GBuffer> m_gbuffer;
    std::unique_ptr<CascadeShadowMap> m_cascadeShadowMap;
    std::unique_ptr<ShaderCache> m_shaderCache;
    std::unique_ptr<GBufferPass> m_gbufferPass;
    std::unique_ptr<ShadowPass> m_shadowPass;
    std::unique_ptr<LightingPass> m_lightingPass;
    std::unique_ptr<GpuCullPass> m_gpuCullPass;
    std::unique_ptr<LightCullPass> m_lightCullPass;
    std::unique_ptr<FrameGraph> m_frameGraph;
    std::unique_ptr<ClusteredLightGrid> m_clusteredLightGrid;
    std::unique_ptr<PostProcessor> m_postProcessor;
    std::unique_ptr<IBL> m_ibl;
    rhi::ITexture* m_iblEnvironmentTexture = nullptr;
    std::unique_ptr<ReflectionProbeManager> m_reflectionProbeManager;
    std::unique_ptr<GpuProfilerRHI> m_gpuProfilerRHI;
    std::shared_ptr<Shader> m_stereoForwardShader;
    std::shared_ptr<Shader> m_skyboxShader;
    std::shared_ptr<Shader> m_proceduralSkyboxShader;
    static constexpr uint32_t kSkyboxPostSetsFrames = 3;
    std::array<rhi::IDescriptorSet*, kSkyboxPostSetsFrames> m_skyboxPostSets = {};
    uint32_t m_lastSkyboxPostSetFilledFrameNumber = 0xffffffffu;  // avoid updating same set twice in one frame
    rhi::ITexture* m_cachedSkyboxTexture = nullptr;
    rhi::ITexture* m_skyboxDummyTexture = nullptr;
    std::shared_ptr<Shader> m_outlinePostShader;
    rhi::IDescriptorSet* m_outlinePostSet = nullptr;
    std::shared_ptr<Shader> m_entityIdShader;
    rhi::IPipeline* m_streamingNV12Pipeline = nullptr;
    rhi::IShader* m_streamingNV12Shader = nullptr;
    rhi::IRenderTarget* m_entityIdRenderTarget = nullptr;
    rhi::IRenderTarget* m_outlineCompositeTemp = nullptr;

    /** Quad mesh for GPU-driven GBuffer path (matches current cull output: 6 indices). */
    std::unique_ptr<Mesh> m_gpuDrivenQuad;

    /** If true, deferred GBuffer uses RenderIndirect when GpuCullPass is supported (E8). */
    bool m_useGpuDrivenGBuffer = false;

    /** A2: Draw list for GpuCullPass (scene-fed); max 256 entries. */
    /** Per-draw transforms for GPU-driven path (same order as draw list). */
    rhi::IBuffer* m_gpuCullTransformBuffer = nullptr;
    /** Scratch for building draw list each frame (A2). */
    std::vector<GpuCullDrawEntry> m_scratchGpuCullDrawList;
    /** Scratch transforms in same order as draw list (for indirect scene path). */
    std::vector<math::Mat4> m_scratchGpuCullTransforms;
    /** True when all opaque drawables share the same mesh (enables indirect scene path). */
    bool m_gpuCullSingleMesh = false;
    /** Representative mesh when single-mesh; kept alive for GBuffer pass. */
    std::shared_ptr<Mesh> m_gpuCullRepresentativeMesh;

    /** Phase D: instance matrix buffer for batched/instanced GBuffer and shadow (mesh+material batching). */
    rhi::IBuffer* m_instanceMatrixBuffer = nullptr;
    static constexpr uint32_t kMaxInstanceBatchSize = 4096u;

    /** Previous frame depth RT for GPU occlusion culling (same size as GBuffer). */
    rhi::IRenderTarget* m_prevFrameDepthRT = nullptr;
    /** True after first frame's depth copy so cull can use occlusion. */
    bool m_havePrevDepth = false;

    /** E9: Bindless heap (created when supportsBindless). */
    rhi::IBindlessHeap* m_bindlessHeap = nullptr;
    /** E9: Cache ITexture* -> BindlessHandle for GetOrAllocateBindlessHandle. */
    std::unordered_map<rhi::ITexture*, rhi::BindlessHandle> m_bindlessHandleCache;

    rhi::IRenderTarget* m_hdrRenderTarget = nullptr;

    // Forward+ SSBOs (RHI)
    rhi::IBuffer* m_pointLightBuffer = nullptr;
    rhi::IBuffer* m_spotLightBuffer = nullptr;
    rhi::IBuffer* m_lightIndexBuffer = nullptr;
    /** Phase 5: single SSBO for deferred lighting (directional + point + spot); filled each frame. */
    rhi::IBuffer* m_sceneLightingBuffer = nullptr;

    /** Resource-space forward pass: Frame, ForwardPass, Material, SceneBuffers sets + Frame UBO. Bindless set from m_bindlessHeap->GetDescriptorSet(). */
    void EnsureForwardResourceSets();
    rhi::IDescriptorSet* m_forwardFrameSet = nullptr;
    rhi::IDescriptorSet* m_forwardPassSet = nullptr;
    rhi::IDescriptorSet* m_forwardMaterialSet = nullptr;
    rhi::IDescriptorSet* m_forwardSceneBuffersSet = nullptr;
    rhi::IDescriptorSet* m_deferredLightingPassSet = nullptr;
    rhi::IBuffer* m_forwardFrameUBO = nullptr;
    rhi::IBuffer* m_forwardBonesBuffer = nullptr;  // SceneBuffers set binding 1 for skinned draws (128 mat4)

    uint32_t m_width = 0, m_height = 0;       // render size (letterbox aspect)
    uint32_t m_windowWidth = 0, m_windowHeight = 0;  // output window size for letterbox blit
    PipelineMode m_mode = PipelineMode::Deferred;
    PrePostCallback m_prePostCallback;
    
    // Previous frame matrices for velocity calculation
    math::Mat4 m_prevView{1.0f};
    math::Mat4 m_prevProjection{1.0f};

    // Forward+ only: scratch for api lights -> internal layout (SSBO std140); drawables are zero-copy
    std::vector<PointLight> m_scratchPointLights;
    std::vector<SpotLight> m_scratchSpotLights;
    // Scratch for transparent drawables when using view (sorting requires a list)
    mutable std::vector<Drawable> m_scratchTransparent;

    // Pending debug lines (per-line color) drawn before post (into HDR) so depth test and tone-mapping apply
    std::vector<std::pair<std::pair<math::Vec3, math::Vec3>, math::Vec4>> m_pendingDebugLinesColored;
    math::Mat4 m_pendingDebugView{1.0f};
    math::Mat4 m_pendingDebugProj{1.0f};

    /** Phase 1: current scene view (for GetSceneForCallback etc.). Set before Execute. */
    const IRenderSceneView* m_currentView = nullptr;
    /** Phase B: current frame data for executors (camera, lights, visibleOpaque, etc.). Set before Execute. */
    const RenderFrameData* m_currentFrameData = nullptr;
    /** Phase B: per-frame snapshot filled by PrepareFrameData. */
    RenderFrameData m_frameData;
    /** Phase 1: compile frame graph only when true (initial, after Resize, or mode/size change). */
    bool m_frameGraphNeedsCompile = true;
    uint32_t m_frameGraphLastWidth = 0;
    uint32_t m_frameGraphLastHeight = 0;
    PipelineMode m_frameGraphLastMode = PipelineMode::Deferred;
    bool m_frameGraphLastStereo = false;
    /** Set each frame by PrepareGpuCullFromView; used by frame-graph GBuffer executor. */
    uint32_t m_lastGpuCullDrawListCount = 0;
    /** Phase 1: stereo path only; set before Execute when building stereo graph. */
    CameraView m_currentLeftCam{};
    CameraView m_currentRightCam{};

#ifndef NDEBUG
    /** Debug guard: set during Render() to catch invalid Resize() mid-frame. */
    bool m_rendering = false;
#endif
};

} // namespace render
} // namespace eyelib
