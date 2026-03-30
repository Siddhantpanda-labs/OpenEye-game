#pragma once

#include "eyelib/render/RenderContext.h"
#include "eyelib/render/IRenderSceneView.h"
#include "eyelib/render/PipelineSharedResources.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/renderer/FrameRenderContext.h"
#include "eyelib/render/renderer/RenderPipelineMode.h"
#include "eyelib/render/renderer/graph/FrameGraphExecutorSet.h"
#include "eyelib/render/renderer/graph/RenderGraphOrchestrator.h"
#include "eyelib/render/passes/GpuCullPassSystem.h"
#include "eyelib/render/passes/GBufferPassOpaqueParams.h"
#include "eyelib/render/passes/ShadowPassParams.h"
#include "eyelib/render/passes/LightCullPassSystem.h"
#include "eyelib/render/rhi/RHI.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eyelib {
namespace render {

class GBuffer;
class CascadeShadowMap;
class ShaderCache;
class ShadowPass;
class LightingPass;
class ClusteredLightGrid;
class PostProcessor;
class SkyboxPassSystem;
class PostProcessPassSystem;
class TransparencyPassSystem;
class DeferredLightingPassSystem;
class ForwardOpaquePassSystem;
class GBufferPassSystem;
class ShadowPassSystem;
class EditorOverlayPassSystem;
class StreamingPassSystem;
class IBL;
class ReflectionProbeManager;
class GpuProfilerRHI;
class Mesh;
class Shader;
class TextureCube;
class FullscreenQuad;
class Material;
class MaterialInstance;
namespace resources {
class PassResourceBuilder;
}

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
    /** Initialize with shared RHI device and reuse shaders/cache from another pipeline. shareFrom must be already initialized. */
    bool Initialize(rhi::IDevice* sharedDevice, uint32_t width, uint32_t height, PipelineSharedResources* shareFrom);

    /** Returns shared resources (populated after first init with shared device). Pass to Initialize(..., shareFrom) for subsequent pipelines. */
    PipelineSharedResources* GetSharedResources() { return m_sharedResources.get(); }
    ShaderCache* GetShaderCache() const { return m_shaderCache; }
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
    FrameGraph* GetFrameGraph() { return m_renderGraphOrchestrator ? m_renderGraphOrchestrator->GetFrameGraph() : nullptr; }
    const std::vector<FrameGraph::PassInfo>& GetFrameGraphPasses() const;

    /**
     * Replace frame-graph executors for one wiring path (tests / tooling). Pass names match `RenderGraphRecipes`
     * (`GPU Cull`, `Shadow`, …); for stereo, also `SkyboxLeft` / `SkyboxRight`. Only listed names are overridden;
     * others keep the built-in `GraphExecute*` behavior.
     */
    void SetFrameGraphExecutorOverrides(FrameGraphExecutorSet set,
                                        std::unordered_map<std::string, FrameGraph::ExecuteFunc> overrides);
    void ClearFrameGraphExecutorOverrides(FrameGraphExecutorSet set);
    void ClearFrameGraphExecutorOverrides();

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
    /** Shared init after m_device is set (owned or shared). When shareFrom is non-null, reuses its shaders/cache. */
    bool InitializeDeviceResources(uint32_t width, uint32_t height, PipelineSharedResources* shareFrom = nullptr);
    void EnsureResources(uint32_t width, uint32_t height);
    void CreateHDRFramebuffer();
    void DestroyHDRFramebuffer();
    /** Frame-graph coordination: compile only when needed (size/mode/stereo change), then execute. */
    void CompileFrameGraphIfNeeded(PipelineMode mode, bool stereo);
    /** Current frame RHI + dimensions for pass builders and post-graph work (uses m_device, m_currentFrameData when set). */
    FrameRenderContext GetFrameRenderContext() const;
    /** Named frame-graph steps; each matches a pass name in RenderGraphRecipes. Uses m_frameGraphExecutionContext during Execute. */
    void GraphExecuteGpuCull();
    void GraphExecuteShadow();
    void GraphExecuteGBufferDeferred();
    void GraphExecuteCopyDepthOcclusionDeferred();
    void GraphExecuteDeferredLighting();
    void GraphExecuteTransparency();
    void GraphExecuteSkybox(bool useGbufferDepth);
    void GraphExecuteForwardOpaque();
    void GraphExecuteCopyDepthOcclusionForward();
    void GraphExecuteForwardPlusOpaque();
    /** Pass name + wiring set → default executor (overrides applied in `ResolveFrameGraphExecutor`). */
    FrameGraph::ExecuteFunc ExecutorFor(FrameGraphExecutorSet set, const std::string& name);
    FrameGraph::ExecuteFunc ExecutorForForwardStereoSkyboxLeft();
    FrameGraph::ExecuteFunc ExecutorForForwardStereoSkyboxRight();
    FrameGraph::ExecuteFunc ResolveFrameGraphExecutor(FrameGraphExecutorSet set, const std::string& name);
    /** Device + command list from frame-graph execution context (B2). */
    RenderContext MakeRenderContext(const FrameRenderContext& frc) const;
    FrameGraph::ExecuteFunc ResolveForwardStereoSkyboxExecutor(bool rightEye);
    /** Pass wiring: delegates to `FrameGraphRecipeBuilders` + executor lookup. */
    void BuildDeferredGraph(FrameGraph& fg);
    void BuildForwardGraph(FrameGraph& fg);
    void BuildForwardStereoGraph(FrameGraph& fg);
    void BuildForwardPlusGraph(FrameGraph& fg);

    void BuildAndExecuteFrameGraph(const IRenderSceneView& view);
    /** Forward mode: frame graph with GPU Cull, Shadow, Forward Opaque, Transparency, Post. */
    void BuildAndExecuteForwardFrameGraph(const IRenderSceneView& view);
    /** Forward+ mode: frame graph with Shadow, Forward+ Opaque (batched), Transparency, Skybox. */
    void BuildAndExecuteForwardPlusFrameGraph(const IRenderSceneView& view);
    /** Same as RenderForwardOpaque but uses an explicit camera and drawable list (stereo per-eye). */
    void RenderForwardOpaqueForCamera(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam,
                                      const std::vector<Drawable>& opaqueDrawables);
    /** Vulkan resource-space forward transparency (bindless + push); OpenGL uses TransparencyPass. */
    void RenderForwardTransparent(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam);
    /** Stereo opaque: Vulkan uses two viewports + forward shader cache + typed push; OpenGL may use forward_stereo + DrawInstanced(2). */
    void RenderForwardOpaqueStereo(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& leftCam, const CameraView& rightCam);
    void BuildAndExecuteForwardFrameGraphStereo(const IRenderSceneView& view, const CameraView& leftCam, const CameraView& rightCam);
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
    /** Build immutable shared forward bindings for a pass (frame/pass/scene sets) before execute. */
    void BuildForwardPassSharedState(const RenderFrameData& frameData, const CameraView& cam, rhi::IDescriptorSet* passSet,
                                     rhi::IDescriptorSet* sceneBuffersSet, bool usePrevViewProj);
    /** Acquire a per-draw material descriptor set for the current frame. */
    rhi::IDescriptorSet* AcquireMaterialDrawSet();
    /** Acquire a per-draw pass descriptor set for the current frame. */
    rhi::IDescriptorSet* AcquirePassDrawSet();
    /** Acquire a per-draw scene descriptor set for the current frame. */
    rhi::IDescriptorSet* AcquireSceneDrawSet();
    /** Acquire a per-pass deferred-lighting descriptor set for the current frame. */
    rhi::IDescriptorSet* AcquireDeferredLightingSet();
    /** Fills current frame's outline Post set with scene+id textures (once per frame/input pair). */
    void EnsureOutlinePostSetFilled(rhi::ITexture* sceneTex, rhi::ITexture* idTex);
    void EnsureOutlinePostShader();
    void EnsureEntityIdShader();
    void EnsureStreamingNV12Pass();
    void RenderEntityIDPass(const IRenderSceneView& view, const std::vector<uint32_t>& entityIdsForOpaque);
    /** Build params for deferred GBuffer opaque pass; consumed by GBufferPassSystem::RenderOpaqueGeometry. */
    GBufferPassOpaqueParams BuildGBufferOpaqueParams(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam,
                                                     uint32_t gpuCullDrawListCount);
    /** Assemble batched shadow payload; consumed by ShadowPassSystem::RenderShadowBatched. */
    ShadowPassBatchedParams BuildShadowBatchedParams(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& camView) const;
    ShadowPassBatchedParams BuildShadowBatchedParams(const ShadowPassBuildParams& build) const;
    struct DeferredLightingPassParams {
        rhi::ICommandList* cmd = nullptr;
        const RenderFrameData* frameData = nullptr;
        CameraView camView{};
        rhi::ClearValue clearColor = { 0.f, 0.f, 0.f, 1.f };
        LightParams lightParams{};
        CascadeShadowMap* cascadeForLighting = nullptr;
        IBL* ibl = nullptr;
        rhi::IDescriptorSet* lightingSetForCall = nullptr;
    };
    struct TransparencyPassParams {
        rhi::IDevice* device = nullptr;
        rhi::ICommandList* cmd = nullptr;
        rhi::IRenderTarget* hdrRenderTarget = nullptr;
        rhi::IRenderTarget* gbufferRenderTarget = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        const RenderFrameData* frameData = nullptr;
        CameraView camView{};
        bool copyDepthFromGBuffer = false;
        bool preferVulkanPath = false;
    };
    struct PostProcessPassParams {
        CameraView camView{};
        const IRenderSceneView* sceneView = nullptr;
        IBL* ibl = nullptr;
        rhi::ITexture* probeTex = nullptr;
        rhi::ITexture* skyboxTex = nullptr;
        rhi::ITexture* hdrTex = nullptr;
        math::Mat4 invView = math::Mat4(1.0f);
        math::Mat4 invProj = math::Mat4(1.0f);
        float time = 0.0f;
        float ssrNearPlane = 0.0f;
        float ssrFarPlane = 0.0f;
    };
    ShadowPassBuildParams BuildShadowPassParams(const FrameRenderContext& frameCtx, const CameraView& camView);
    DeferredLightingPassParams BuildDeferredLightingPassParams(const FrameRenderContext& frameCtx, const CameraView& camView,
                                                               const rhi::ClearValue& clearColor);
    TransparencyPassParams BuildTransparencyPassParams(const FrameRenderContext& frameCtx, const CameraView& camView);
    PostProcessPassParams BuildPostProcessPassParams(const FrameRenderContext& frameCtx, const IRenderSceneView* sceneView, const CameraView& camView);
    void ExecuteShadowPass(const ShadowPassBuildParams& buildParams);
    void ExecuteDeferredLightingPass(const DeferredLightingPassParams& params);
    void ExecuteTransparencyPass(const TransparencyPassParams& params);
    void ExecutePostProcessPass(const PostProcessPassParams& params);

    GpuCullScenePrepUploadParams BuildGpuCullScenePrepUploadParams(const CameraView& cam) const;
    /** Shared: build draw list from view, upload to GPU, configure GpuCullPass. Returns draw list count. Used by deferred and forward frame graphs. */
    uint32_t PrepareGpuCullFromView(const IRenderSceneView& view, const CameraView& cam);
    /** Fill frame data from view (camera, lights, visible/all opaque, transparent). Call once per frame before graph execute. */
    void PrepareFrameData(const IRenderSceneView& view);
    /** Build GPU cull draw list from frame data visible opaque. Call after PrepareFrameData. */
    uint32_t PrepareGpuCullFromFrameData(const RenderFrameData& frameData, const CameraView& cam);
    /** Ensure IBL is built from frameData.skybox; returns m_ibl.get() when valid, else nullptr. Used by Deferred Lighting, Forward, Forward+. */
    IBL* EnsureIBLFromFrameData(const RenderFrameData& frameData);
    /** Transition persistent textures (IBL/probe) to shader-read once per frame, before frame-graph pass rendering begins. */
    void PreparePersistentShaderReadInputs(rhi::ICommandList* cmd, const RenderFrameData& frameData);

    rhi::IDevice* m_device = nullptr;
    std::unique_ptr<rhi::IDevice> m_ownedDevice;
    std::unique_ptr<PipelineSharedResources> m_sharedResources;  // Populated on first init; used when shareFrom is null
    PipelineSharedResources* m_usingShared = nullptr;            // Non-null when we borrow from another pipeline
    std::unique_ptr<GBuffer> m_gbuffer;
    std::unique_ptr<CascadeShadowMap> m_cascadeShadowMap;
    /** Points to our or shared ShaderCache. Use GetShaderCache(). */
    ShaderCache* m_shaderCache = nullptr;
    std::unique_ptr<ShadowPass> m_shadowPass;
    std::unique_ptr<LightingPass> m_lightingPass;
    /** Forward+ GPU light cull compute; grid + light SSBOs stay on pipeline. */
    std::unique_ptr<LightCullPassSystem> m_lightCullPassSystem;
    std::unique_ptr<SkyboxPassSystem> m_skyboxPassSystem;
    std::unique_ptr<PostProcessPassSystem> m_postProcessPassSystem;
    std::unique_ptr<TransparencyPassSystem> m_transparencyPassSystem;
    std::unique_ptr<DeferredLightingPassSystem> m_deferredLightingPassSystem;
    std::unique_ptr<ForwardOpaquePassSystem> m_forwardOpaquePassSystem;
    std::unique_ptr<GBufferPassSystem> m_gbufferPassSystem;
    /** GpuCullPass + transform buffer + CPU scratch for draw list / indirect path. */
    std::unique_ptr<GpuCullPassSystem> m_gpuCullPassSystem;
    std::unique_ptr<ShadowPassSystem> m_shadowPassSystem;
    std::unique_ptr<EditorOverlayPassSystem> m_editorOverlayPassSystem;
    std::unique_ptr<StreamingPassSystem> m_streamingPassSystem;
    /** Frame graph + compile scheduling (size / mode / stereo). */
    std::unique_ptr<RenderGraphOrchestrator> m_renderGraphOrchestrator;
    std::unique_ptr<ClusteredLightGrid> m_clusteredLightGrid;
    std::unique_ptr<PostProcessor> m_postProcessor;
    std::unique_ptr<IBL> m_ibl;
    rhi::ITexture* m_iblEnvironmentTexture = nullptr;
    std::unique_ptr<ReflectionProbeManager> m_reflectionProbeManager;
    std::unique_ptr<GpuProfilerRHI> m_gpuProfilerRHI;
    std::shared_ptr<Shader> m_stereoForwardShader;
    std::shared_ptr<Shader> m_outlinePostShader;
    static constexpr uint32_t kOutlinePostSetsFrames = 3;
    std::array<rhi::IDescriptorSet*, kOutlinePostSetsFrames> m_outlinePostSets = {};
    uint32_t m_lastOutlinePostSetFilledFrameNumber = 0xffffffffu;
    rhi::ITexture* m_cachedOutlineSceneTexture = nullptr;
    rhi::ITexture* m_cachedOutlineIdTexture = nullptr;
    std::shared_ptr<Shader> m_entityIdShader;
    rhi::IPipeline* m_streamingNV12Pipeline = nullptr;
    rhi::IShader* m_streamingNV12Shader = nullptr;
    rhi::IRenderTarget* m_entityIdRenderTarget = nullptr;
    rhi::IRenderTarget* m_outlineCompositeTemp = nullptr;

    /** Quad mesh for GPU-driven GBuffer path (matches current cull output: 6 indices). */
    std::unique_ptr<Mesh> m_gpuDrivenQuad;

    /** If true, deferred GBuffer uses RenderIndirect when GpuCullPass is supported (E8). */
    bool m_useGpuDrivenGBuffer = false;

    /** Phase D: instance matrix buffer for batched/instanced GBuffer and shadow (mesh+material batching). */
    rhi::IBuffer* m_instanceMatrixBuffer = nullptr;

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

    void EnsureCurrentFrameResourceSets();
    std::unique_ptr<resources::PassResourceBuilder> m_passResourceBuilder;

    /** Current frame aliases into per-frame arrays (updated by EnsureCurrentFrameResourceSets from PassResourceBuilder::Current). */
    rhi::IDescriptorSet* m_frameSet = nullptr;
    rhi::IDescriptorSet* m_passSet = nullptr;
    rhi::IDescriptorSet* m_transparentPassSet = nullptr;
    rhi::IDescriptorSet* m_materialSet = nullptr;
    rhi::IDescriptorSet* m_sceneSet = nullptr;
    rhi::IDescriptorSet* m_transparentSceneSet = nullptr;
    rhi::IDescriptorSet* m_deferredLightingSet = nullptr;
    rhi::IBuffer* m_frameUbo = nullptr;
    rhi::IBuffer* m_bonesBuffer = nullptr;  // SceneBuffers set binding 1 for skinned draws (128 mat4)

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
    /** Populated immediately before frame graph Execute; consumed by GraphExecute* and stereo graph lambdas. Cleared after Execute. */
    FrameRenderContext m_frameGraphExecutionContext{};
    /** Phase B: per-frame snapshot filled by PrepareFrameData. */
    RenderFrameData m_frameData;
    /** Set each frame by PrepareGpuCullFromFrameData / PrepareGpuCullFromView; used by GBuffer path. */
    uint32_t m_lastGpuCullDrawListCount = 0;
    /** Optional per-path overrides for frame-graph pass executors (see SetFrameGraphExecutorOverrides). */
    std::map<FrameGraphExecutorSet, std::unordered_map<std::string, FrameGraph::ExecuteFunc>> m_frameGraphExecutorOverrides;

    /** Phase 1: stereo path only; set before Execute when building stereo graph. */
    CameraView m_currentLeftCam{};
    CameraView m_currentRightCam{};
    CameraView m_currentLeftCamRaw{};
    CameraView m_currentRightCamRaw{};

#ifndef NDEBUG
    /** Debug guard: set during Render() to catch invalid Resize() mid-frame. */
    bool m_rendering = false;
#endif
};

} // namespace render
} // namespace eyelib
