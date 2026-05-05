#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"
#include "eyelib/rendering/core/RenderBackend.h"
#include "eyelib/rendering/framegraph/FrameGraphAttachmentOps.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Opaque init info for ImGui Vulkan backend. Cast void* fields to VkInstance, VkDevice, etc. when calling ImGui_ImplVulkan_Init. */
struct ImGuiVulkanInitInfo {
    void* instance = nullptr;
    void* physical_device = nullptr;
    void* device = nullptr;
    uint32_t queue_family = 0;
    void* queue = nullptr;
    void* pipeline_cache = nullptr;
    void* descriptor_pool = nullptr;
    uint32_t min_image_count = 2;
    uint32_t image_count = 0;
    void* render_pass = nullptr;
};

// Forward declarations
struct IDevice;
struct ICommandList;
struct IBuffer;
struct ITexture;
struct IShader;
struct IPipeline;
struct IPipelineLayout;
struct IDescriptorSet;
struct IBindlessHeap;
struct IRenderTarget;
struct IQueryPool;
struct IFence;
struct IAccelerationStructure;
/** Scratch region for acceleration-structure GPU builds (see `AccelerationStructureScratchBufferDesc`). */
struct IAccelerationStructureScratchBuffer {
    virtual ~IAccelerationStructureScratchBuffer() = default;
    virtual IBuffer* Buffer() = 0;
    virtual const IBuffer* Buffer() const = 0;
};
/** TLAS instance record array (see `AccelerationStructureInstanceBufferDesc`). */
struct IAccelerationStructureInstanceBuffer {
    virtual ~IAccelerationStructureInstanceBuffer() = default;
    virtual IBuffer* Buffer() = 0;
    virtual const IBuffer* Buffer() const = 0;
    virtual uint32_t GetMaxInstanceCount() const = 0;
    virtual size_t GetInstanceStrideBytes() const = 0;
};

/** GPU frame lifecycle for backends that submit per-frame command buffers (Vulkan). Used for debug checks and tooling. */
enum class GpuFramePhase : uint8_t {
    BetweenFrames,
    Recording,
    Submitting,
};

// ---- Device ----

struct IDevice {
    virtual ~IDevice() = default;

    /** Backend capabilities; use these instead of branching on backend type. */
    virtual const RHICapabilities& GetCapabilities() const = 0;
    /** Vulkan: true after `VK_ERROR_DEVICE_LOST` on a tracked queue/surface path; `VkDevice` and child handles are invalid until teardown and full re-init. Default false. */
    virtual bool IsDeviceLost() const { return false; }

    /** Typed backend identity (avoid string matching). */
    virtual ::eyelib::rendering::RenderBackend GetRenderBackend() const = 0;

    /** Shader path resolution: Vulkan uses assets/shaders/rendering/vulkan/{logicalPath}; other backends use assets/shaders/{backendName}/{logicalPath}. Extensions are backend-specific (e.g. OpenGL: .vert/.frag; Diligent: .vsh/.psh). */
    virtual const char* GetShaderBackendName() const = 0;
    virtual const char* GetShaderVertexExtension() const = 0;
    virtual const char* GetShaderFragmentExtension() const = 0;
    /** Resolve logical shader path (e.g. raster/forward/forward_opaque) to backend-specific asset path. Returns true and sets outPath on success. Used for compute and other single-file shaders. */
    virtual bool ResolveShaderPath(const char* logicalName, ShaderStage stage, std::string& outPath) { (void)logicalName; (void)stage; (void)outPath; return false; }

    /** Allocate a command list for recording. Begin/End/record on it, then SubmitCommandList(list).
     *  RHI is designed for Vulkan-style create/record/submit. OpenGL backend: CreateCommandList() returns
     *  a deferred list that records to CPU; SubmitCommandList(list) replays it on the immediate list (render thread).
     *  Multithreaded recording: use one command list per worker thread; CreateCommandList() is safe from any thread.
     *  Only one thread may record to a given list at a time. */
    virtual ICommandList* CreateCommandList() = 0;
    /** Release a command list from CreateCommandList. No-op for nullptr. Do not destroy a list while another thread is recording to it. */
    virtual void DestroyCommandList(ICommandList* list) = 0;

    virtual ICommandList* GetImmediateCommandList() = 0;
    /** Top of the transient stack when non-empty; pipeline recording and Blit use this instead of the immediate buffer (Vulkan editor slices). Nested scopes use Push/Pop. */
    virtual ICommandList* GetTransientRecordingCommandList() const { return nullptr; }
    /** Push a slice command list after Begin(); becomes active until Pop. Must pair with PopTransientRecordingCommandList (same pointer). */
    virtual void PushTransientRecordingCommandList(ICommandList* cmd) { (void)cmd; }
    /** Pop the active transient list; @param cmd must match the list pushed by the innermost scope. */
    virtual void PopTransientRecordingCommandList(ICommandList* cmd) { (void)cmd; }
    /** Same buffer as RenderPipeline::GetRHICommandList(): transient slice when set, else immediate. Use for all per-frame GPU recording that must stay on one command buffer. */
    virtual ICommandList* GetActiveRecordingCommandList() {
        ICommandList* t = GetTransientRecordingCommandList();
        return t ? t : GetImmediateCommandList();
    }
    /** Ownership transfers to the device; buffer is submitted before the compose buffer in the same queue submit. List must be End()ed. Vulkan editor only. */
    virtual void EnqueueFrameSliceCommandList(ICommandList* cmd) { (void)cmd; }
    /** Replay a deferred list (or no-op for immediate). Must be called on the render thread. Submit lists in order for deterministic execution. */
    virtual void SubmitCommandList(ICommandList* list) = 0;
    /** Vulkan: override deferred-list queue routing (`AsyncComputeSubmitPolicy`). Other backends: no-op / default. */
    virtual void SetAsyncComputeSubmitPolicy(AsyncComputeSubmitPolicy policy) { (void)policy; }
    virtual AsyncComputeSubmitPolicy GetAsyncComputeSubmitPolicy() const { return AsyncComputeSubmitPolicy::Automatic; }

    virtual IBuffer* CreateBuffer(const BufferDesc& desc, const void* initialData = nullptr) = 0;
    virtual void DestroyBuffer(IBuffer* buffer) = 0;
    /** Vulkan: returns `vkGetBufferDeviceAddress` for buffers created with shader-device-address usage; 0 when unsupported/unavailable. */
    virtual uint64_t GetBufferDeviceAddress(IBuffer* buffer) { (void)buffer; return 0; }

    /** Vulkan `VK_KHR_acceleration_structure`: query build sizes for a device-side build. Fills zeros when unsupported. */
    virtual bool GetAccelerationStructureBuildSizes(const AccelerationStructureBuildSizesInput& input, AccelerationStructureBuildSizes& outSizes) {
        (void)input;
        outSizes = {};
        return false;
    }
    /** Vulkan `VK_KHR_acceleration_structure`: create a `VkAccelerationStructureKHR` in a buffer subrange. Returns nullptr when unsupported or on failure. */
    virtual IAccelerationStructure* CreateAccelerationStructure(const AccelerationStructureDesc& desc) { (void)desc; return nullptr; }
    /** Vulkan: deferred `vkDestroyAccelerationStructureKHR` (GPU fence); destroy **before** freeing `AccelerationStructureDesc::storageBuffer`. */
    virtual void DestroyAccelerationStructure(IAccelerationStructure* as) { (void)as; }

    /** Vulkan: scratch buffer for `vkCmdBuildAccelerationStructuresKHR` (storage + shader device address, scratch-aligned). */
    virtual IAccelerationStructureScratchBuffer* CreateAccelerationStructureScratchBuffer(const AccelerationStructureScratchBufferDesc& desc,
                                                                                          const void* initialData = nullptr) {
        (void)desc;
        (void)initialData;
        return nullptr;
    }
    virtual void DestroyAccelerationStructureScratchBuffer(IAccelerationStructureScratchBuffer* buffer) {
        if (!buffer) return;
        DestroyBuffer(buffer->Buffer());
    }
    /** Vulkan: TLAS instance buffer (`VkAccelerationStructureInstanceKHR` stride). */
    virtual IAccelerationStructureInstanceBuffer* CreateAccelerationStructureInstanceBuffer(const AccelerationStructureInstanceBufferDesc& desc,
                                                                                              const void* initialData = nullptr) {
        (void)desc;
        (void)initialData;
        return nullptr;
    }
    virtual void DestroyAccelerationStructureInstanceBuffer(IAccelerationStructureInstanceBuffer* buffer) {
        if (!buffer) return;
        DestroyBuffer(buffer->Buffer());
    }

    virtual ITexture* CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) = 0;
    virtual void DestroyTexture(ITexture* texture) = 0;
    /** Commit or decommit a region of a sparse texture. No-op if texture is not sparse or backend does not support. */
    virtual void CommitSparseTexturePages(ITexture* texture, uint32_t level, int32_t x, int32_t y, int32_t z, uint32_t width, uint32_t height, uint32_t depth, bool commit) = 0;
    /** Commit or decommit a region of a sparse buffer. No-op if buffer is not sparse or backend does not support. */
    virtual void CommitSparseBufferPages(IBuffer* buffer, size_t offset, size_t size, bool commit) = 0;
    /** Create a texture view (alias) of an existing texture with optional format/range. Returns nullptr if unsupported. */
    virtual ITexture* CreateTextureView(ITexture* source, RHITextureFormat viewFormat, uint32_t baseMipLevel, uint32_t mipLevels, uint32_t baseLayer, uint32_t layerCount) = 0;
    /** Create a buffer-backed texture (TBO) for sampling buffer data. Returns nullptr if unsupported. */
    virtual ITexture* CreateTextureBuffer(IBuffer* buffer, RHITextureFormat format, size_t offset, size_t size) = 0;

    virtual IShader* CreateShader(ShaderStage stage, const char* source, size_t sourceLen) = 0;
    /** Load shader from precompiled binary. Backends that don't support it return nullptr. */
    virtual IShader* CreateShaderFromBinary(ShaderStage stage, const void* data, size_t size) = 0;
    /** Try to load vertex and fragment shaders from backend-specific paths. Returns true and sets outVs/outFs; caller must DestroyShader. Returns false if unsupported or not found. */
    virtual bool TryLoadShaderFromPaths(const char* vertexPath, const char* fragmentPath, IShader*& outVs, IShader*& outFs) = 0;
    virtual void DestroyShader(IShader* shader) = 0;

    virtual IPipeline* CreateGraphicsPipeline(const PipelineDesc& desc) = 0;
    /** Create a graphics pipeline from a logical shader asset (SPIR-V and/or GLSL resolution is backend-specific). */
    virtual IPipeline* CreateGraphicsPipelineFromShaderAsset(const ShaderGraphicsPipelineCreateInfo& info) = 0;
    /** E5.3: Create pipeline from cached binary (format + data from GetPipelineBinary). Returns nullptr if unsupported or invalid. */
    virtual IPipeline* CreateGraphicsPipelineFromBinary(const void* data, size_t size, uint32_t binaryFormat, const VertexLayout& layout, PrimitiveTopology topology) = 0;
    /** E5.3: Retrieve pipeline binary for caching. Returns false if unsupported; outFormat and outData set on success. */
    virtual bool GetPipelineBinary(IPipeline* pipeline, std::vector<uint8_t>& outData, uint32_t& outFormat) = 0;
    virtual IPipeline* CreateComputePipeline(void* computeShader) = 0;
    /** Create compute pipeline with resource-space layout (e.g. ComputeLightCull, ComputeGpuCull). Default impl: CreateComputePipeline(shader) (Legacy). */
    virtual IPipeline* CreateComputePipelineWithLayout(void* computeShader, PipelineLayoutKind layoutKind) { (void)layoutKind; return CreateComputePipeline(computeShader); }
    virtual void DestroyPipeline(IPipeline* pipeline) = 0;
    /** OpenGL: set SSBO block bindings on the pipeline's program (e.g. IndirectOut->0, DrawListIn->1). No-op on other backends. Call before first use to avoid interface mismatch (GL 1282). */
    virtual void EnsureComputePipelineStorageBlockBindings(IPipeline* pipeline) {}
    /** Returns true if the compute pipeline has an active SSBO block with the given name. Only bind to a slot when this returns true to avoid GL 1282 (binding to non-existent slot). Default: true. */
    virtual bool ComputePipelineHasStorageBlock(IPipeline* pipeline, const char* blockName) { (void)pipeline; (void)blockName; return true; }

    virtual IRenderTarget* CreateRenderTarget(uint32_t width, uint32_t height,
                                              const std::vector<RHITextureFormat>& colorFormats,
                                              RHITextureFormat depthFormat,
                                              uint32_t samples = 1,
                                              bool transient = false) = 0;
    /** Depth-only render target with 2D array depth texture (for cascade shadow maps). Use SetDepthLayer then SetRenderTarget to render to each layer. */
    virtual IRenderTarget* CreateDepthOnlyRenderTargetArray(uint32_t width, uint32_t height, uint32_t arrayLayers) { (void)width; (void)height; (void)arrayLayers; return nullptr; }
    /** Create render target with color attachment(s) and shared depth (e.g. entity ID pass reusing main depth). sharedDepthTexture must outlive the RT. */
    virtual IRenderTarget* CreateRenderTargetWithSharedDepth(uint32_t width, uint32_t height,
                                                             const std::vector<RHITextureFormat>& colorFormats,
                                                             ITexture* sharedDepthTexture,
                                                             uint32_t samples = 1) = 0;
    /** Create render target from existing texture (face for cubemap, mip level). */
    virtual IRenderTarget* CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) = 0;
    /** Create render target from cubemap face with shared depth (e.g. for reflection probe capture). depthTexture must be 2D, same size as face. */
    virtual IRenderTarget* CreateRenderTargetFromTextureWithDepth(ITexture* colorTexture, uint32_t face, uint32_t mipLevel, ITexture* depthTexture) {
        (void)colorTexture; (void)face; (void)mipLevel; (void)depthTexture;
        return nullptr;
    }
    virtual void DestroyRenderTarget(IRenderTarget* rt) = 0;

    /** Lazy 1×1×1 depth-array RT for sampler2DArray shadow slots (forward + deferred). Device-owned; do not DestroyRenderTarget. */
    virtual IRenderTarget* GetSharedShadowMapArrayDummyRenderTarget() { return nullptr; }
    /** Transition shared dummy depth to shader-read once (never rendered as attachment). No-op if unsupported or already done. */
    virtual void EnsureSharedShadowMapArrayDummyForShaderSampling(ICommandList* cmd) { (void)cmd; }

    /** @param pipelineStatisticsMask bits compatible with `VkQueryPipelineStatisticFlagBits` when `type == QueryType::PipelineStatistics`; `0` uses `kDefaultPipelineStatisticsFlags`. Ignored for other types. */
    virtual IQueryPool* CreateQueryPool(QueryType type, uint32_t count, PipelineStatisticsFlags pipelineStatisticsMask = 0) = 0;
    virtual void DestroyQueryPool(IQueryPool* pool) = 0;
    /** Vulkan: `vkResetQueryPool` on the host when `GetCapabilities().supportsHostQueryReset`. No-op otherwise.
     *  Caller must ensure each query in `[firstQuery, firstQuery+queryCount)` is in the **unavailable** state (synchronize with GPU) per the Vulkan spec. */
    virtual void ResetQueryPoolOnHost(IQueryPool* pool, uint32_t firstQuery, uint32_t queryCount) {
        (void)pool;
        (void)firstQuery;
        (void)queryCount;
    }

    /** Create pipeline layout (describes bindings for descriptor sets). Used for Vulkan VkPipelineLayout; OpenGL emulates. */
    virtual IPipelineLayout* CreatePipelineLayout(const PipelineLayoutDesc& desc) = 0;
    virtual void DestroyPipelineLayout(IPipelineLayout* layout) = 0;
    /** Create descriptor set from layout; fill with UpdateTexture/UpdateUniformBuffer/UpdateStorageBuffer, then BindDescriptorSet(cmd, set). */
    virtual IDescriptorSet* CreateDescriptorSet(IPipelineLayout* layout) = 0;
    virtual void DestroyDescriptorSet(IDescriptorSet* set) = 0;
    /** Resource-space set type for binding table (set 0 Frame, set 1 Pass, etc.). Get layout then CreateDescriptorSet(layout). Returns nullptr if unsupported. */
    enum class ResourceSetType : uint32_t {
        Frame,
        PostPass,
        ForwardPass,
        ShadowPass,
        LightCullPass,
        Material,
        SceneBuffers,
        DeferredLightingPass,
        IBLPass,
        OutlinePost,
        RasterForwardData,
        RasterGBufferData,
        RasterDeferredLightingData,
    };
    virtual IPipelineLayout* GetResourceSetLayout(ResourceSetType type) { (void)type; return nullptr; }
    /** Vulkan resource-space: empty descriptor set (0 bindings) for placeholder slots 1–3 in compute layouts. Returns nullptr if unsupported. */
    virtual IDescriptorSet* GetEmptyResourceSet() { return nullptr; }

    /** Create bindless heap for allocate-once bindless texture/buffer handles. Returns nullptr if !GetCapabilities().supportsBindless. */
    virtual IBindlessHeap* CreateBindlessHeap() = 0;
    virtual void DestroyBindlessHeap(IBindlessHeap* heap) = 0;
    /** Device-global bindless heap shared by all pipelines on this device. Default returns nullptr for backends without bindless support. */
    virtual IBindlessHeap* GetOrCreateSharedBindlessHeap() { return nullptr; }
    /** Device-global texture handle allocation/caching. Returns kInvalidBindlessHandle when unsupported or allocation failed. */
    virtual BindlessHandle GetOrAllocateSharedBindlessTextureHandle(ITexture* texture) { (void)texture; return kInvalidBindlessHandle; }

    /** Maximum supported MSAA samples; clamp requested samples to this. Returns 0 if no MSAA support. */
    virtual int GetMaxSamples(int requestedSamples) = 0;

    /** Wrap an existing backend texture (e.g. GL id as void*) for use with BindTexture. Caller owns the GL texture; wrapper is not destroyed with glDelete. */
    virtual ITexture* CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) = 0;

    /** Start frame: device owns command buffer lifetime. Vulkan: reset and begin immediate command buffer. Call once per frame before any recording. Other backends: no-op. */
    virtual void BeginFrame() {}
    /** End frame: end recording and submit. Vulkan: end immediate command buffer and submit. Call after all recording (scene, blit, UI) and before Present(). Other backends: no-op. */
    virtual void EndFrame() {}
    /** Vulkan: BetweenFrames → Recording → Submitting → BetweenFrames. Other backends: BetweenFrames. */
    virtual GpuFramePhase GetGpuFramePhase() const { return GpuFramePhase::BetweenFrames; }
    /** Current frame index (0 .. N-1) for per-frame resources. Vulkan: ring index for DeletionQueue/descriptor sets. Default 0 for backends without multiple frames in flight. */
    virtual uint32_t GetCurrentFrameIndex() const { return 0; }
    /** Monotonic frame number (incremented each BeginFrame). Use to avoid updating a descriptor set more than once per frame. Default 0. */
    virtual uint32_t GetFrameNumber() const { return 0; }
    virtual void Present() = 0;
    /** Destroy descriptor set after in-flight work is done (Vulkan: deferred). Default: destroy immediately. */
    virtual void EnqueueDestroyDescriptorSet(IDescriptorSet* set) { if (set) DestroyDescriptorSet(set); }
    /** Notify backend that output window framebuffer size changed; backend may recreate swapchain/default backbuffer resources. */
    virtual void OnWindowResized(uint32_t width, uint32_t height) { (void)width; (void)height; }
    /** Vulkan: recreate swapchain with the given present mode when possible. Other backends: no-op.
     *  Prefer `GetCapabilities().supportsPresentMailbox` / `supportsPresentImmediate` before requesting Mailbox/Immediate. */
    virtual void SetSwapchainPresentMode(SwapchainPresentMode mode) { (void)mode; }
    virtual SwapchainPresentMode GetSwapchainPresentMode() const { return SwapchainPresentMode::Fifo; }
    /** Vulkan: request HDR/wide-gamut swapchain when `VK_EXT_swapchain_colorspace` + surface support allow (recreates swapchain). */
    virtual void SetSwapchainOutputMode(SwapchainOutputMode mode) { (void)mode; }
    virtual SwapchainOutputMode GetSwapchainOutputMode() const { return SwapchainOutputMode::SdrSrgb; }
    /** Vulkan: current swapchain `VkFormat` / `VkColorSpaceKHR` (as uint32_t); zeros if no swapchain or non-Vulkan. */
    virtual void GetSwapchainSurfaceColorInfo(uint32_t* outVkFormat, uint32_t* outVkColorSpaceKHR) const {
        if (outVkFormat) *outVkFormat = 0;
        if (outVkColorSpaceKHR) *outVkColorSpaceKHR = 0;
    }
    /** Vulkan: block until `presentId` is processed (requires `supportsPresentWait`). Timeout in nanoseconds (`UINT64_MAX` = wait indefinitely). */
    virtual bool WaitForPresent(uint64_t presentId, uint64_t timeoutNs = UINT64_MAX) { (void)presentId; (void)timeoutNs; return false; }
    /** Vulkan: `WaitForPresent(GetLastPresentId(), timeoutNs)` when present-wait is enabled and a present id was recorded; otherwise false. If last id is 0, returns true (nothing to wait on). */
    virtual bool WaitForPreviousPresent(uint64_t timeoutNs = UINT64_MAX) { (void)timeoutNs; return false; }
    /** Vulkan: last `VkPresentIdKHR` submitted to `vkQueuePresentKHR` (0 if present-id path off). */
    virtual uint64_t GetLastPresentId() const { return 0; }
    /** Wait until all previously submitted GPU work has completed (e.g. glFinish). Use before swap to avoid flicker from presenting before blit completes. */
    virtual void Finish() {}
    /** Optional: barrier tracker for batched transitions (e.g. Vulkan BarrierTrackerVK). Returns nullptr if not supported. */
    virtual void* GetBarrierTracker() { return nullptr; }
    /** Optional: transition texture to state via barrier tracker. No-op if GetBarrierTracker() is null. Used by FrameGraph for batched barriers. */
    virtual void TransitionTextureToState(ITexture* texture, ResourceState newState) { (void)texture; (void)newState; }
    /** Optional: whole-image layout cached in the barrier tracker (`Undefined` if unknown / unsupported). Used at graph compile for cross-pass merge. */
    virtual ResourceState GetTextureTrackedState(const ITexture* texture) const { (void)texture; return ResourceState::Undefined; }
    /** Optional: flush pending barrier tracker transitions into the command list. No-op if GetBarrierTracker() is null. */
    virtual void FlushBarrierTracker(ICommandList* cmd) { (void)cmd; }
    /** Blit color from given render target to the default framebuffer (e.g. for present). */
    virtual void BlitToDefaultFramebuffer(IRenderTarget* src) = 0;
    /** Optional: call after blit, before present (e.g. to record ImGui). Vulkan: receives current VkCommandBuffer. No-op on other backends. */
    virtual void SetPostBlitCallback(std::function<void(void*)> cb) { (void)cb; }
    /** Fill init info for ImGui Vulkan backend. Returns false if backend is not Vulkan or not ready. */
    virtual bool GetImGuiVulkanInitInfo(ImGuiVulkanInitInfo* out) { (void)out; return false; }
    /** Blit from backend-native framebuffer (e.g. GL FBO id as void*) to default framebuffer. */
    virtual void BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) = 0;
    /** Blit full contents of src into a subregion of the default framebuffer (e.g. for stereo split: left/right halves). dstX,dstY,dstW,dstH in window coordinates. */
    virtual void BlitToDefaultFramebufferSubregion(IRenderTarget* src, int dstX, int dstY, int dstW, int dstH) = 0;
    /** Clear default framebuffer then blit src into a centered rect that preserves aspect ratio (letterbox). windowWidth/Height = output size. */
    virtual void BlitToDefaultFramebufferLetterbox(IRenderTarget* src, uint32_t windowWidth, uint32_t windowHeight) = 0;
    /** Blit from src to dst (full size). colorOnly: true = color bit, false = color + depth. */
    virtual void Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) = 0;
    /** Blit from backend-native framebuffer (e.g. GL FBO id as void*) to RHI render target. */
    virtual void BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) = 0;
    /** Blit only depth from src to dst (e.g. GBuffer depth to HDR RT for transparency). Backend may no-op if not supported. */
    virtual void BlitDepthOnly(IRenderTarget* src, IRenderTarget* dst) = 0;
    /** Read one pixel from RT's color attachment 0 (must be R32UI). (x,y) in RT coordinates, origin bottom-left. Call after frame is done (e.g. Finish). */
    virtual bool ReadPixelUint32(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t* outValue) = 0;
    /** Read color pixels from RT's color attachment 0 into RGBA8. (x,y,w,h) in RT coords, origin bottom-left. outBuffer must be at least w*h*4 bytes. Call after frame is done (e.g. Finish). */
    virtual bool ReadPixelsRGBA8(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t* outBuffer) = 0;
    /** Async readback: create a pixel pack buffer for glReadPixels into PBO. Returns opaque handle or nullptr if unsupported. */
    virtual void* CreatePixelPackBuffer(size_t sizeInBytes) { (void)sizeInBytes; return nullptr; }
    virtual void DestroyPixelPackBuffer(void* handle) { (void)handle; }
    /** Issue async readback from RT (RGBA8) into PBO. Does not block; map the PBO later to get data. */
    virtual void ReadPixelsToPBO(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, void* pboHandle) { (void)rt; (void)x; (void)y; (void)w; (void)h; (void)pboHandle; }
    /** Map PBO and copy to outBuffer (top-left row order). Blocks until transfer into PBO is complete. sizeInBytes = w*h*4. */
    virtual void MapPixelPackBuffer(void* pboHandle, void* outBuffer, size_t sizeInBytes, uint32_t width, uint32_t height) { (void)pboHandle; (void)outBuffer; (void)sizeInBytes; (void)width; (void)height; }
    /** Map PBO and copy to outBuffer without row flip (raw bytes; for NV12). Blocks until transfer complete. */
    virtual void MapPixelPackBufferRaw(void* pboHandle, void* outBuffer, size_t sizeInBytes) { (void)pboHandle; (void)outBuffer; (void)sizeInBytes; }
    /** Wait for fence (CPU blocks until GPU has passed the fence). timeoutMs: 0 = no wait, UINT64_MAX = wait forever. Returns true if signalled. */
    virtual bool WaitFence(IFence* fence, uint64_t timeoutMs) = 0;
    virtual void DestroyFence(IFence* fence) = 0;
};

// ---- Command list ----

struct ICommandList {
    virtual ~ICommandList() = default;

    virtual void Begin() = 0;
    virtual void End() = 0;
    /** True between Begin() and End(). Used so Render() can skip Begin when VulkanRenderSliceScope (or similar) already opened recording on the active list. */
    virtual bool IsRecording() const { return false; }

    virtual void SetRenderTarget(IRenderTarget* rt) = 0;
    virtual void SetRenderTarget(IRenderTarget* rt, const ::eyelib::rendering::RenderAttachmentOps& ops) { (void)ops; SetRenderTarget(rt); }
    virtual void SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) = 0;
    virtual void ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) = 0;
    /** Clear only color attachment(s); depth/stencil remain untouched (for RTs with shared depth). */
    virtual void ClearRenderTargetColorOnly(IRenderTarget* rt, const ClearValue& clear) { ClearRenderTarget(rt, clear); }
    virtual void ClearDefaultFramebuffer(const ClearValue& clear) = 0;
    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    /** Set viewport for a given index (for stereo: index 0 = left, 1 = right). Used with gl_ViewportIndex in shaders. */
    virtual void SetViewportIndexed(uint32_t index, uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    virtual void SetDepthTest(bool enable) = 0;
    virtual void SetDepthWrite(bool enable) = 0;
    virtual void SetDepthFuncLess(bool less) = 0;  // true = GL_LESS, false = GL_LEQUAL etc.
    virtual void SetCullFace(bool enable) = 0;
    /** When cull face is enabled: true = cull front faces (e.g. shadow map), false = cull back. */
    virtual void SetCullFrontFace(bool cullFront) = 0;
    /** Enable/disable alpha blending. Factors set by SetBlendFunc (default SrcAlpha, OneMinusSrcAlpha). */
    virtual void SetBlend(bool enable) = 0;
    /** Blend factors when blending is enabled. Opaque pass should use SetBlend(false). */
    virtual void SetBlendFunc(BlendFactor src, BlendFactor dst) = 0;
    /** Polygon offset for lines (GL_POLYGON_OFFSET_LINE + glPolygonOffset). Bias line depth toward camera; use negative factor/units so wireframe wins when co-planar but stays occluded when behind. Other backends may no-op. */
    virtual void SetPolygonOffsetLine(bool enable, float factor, float units) = 0;

    virtual void BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) = 0;
    virtual void BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) = 0;
    /** After compute writing to SSBO: use only GL_SHADER_STORAGE_BARRIER_BIT (minimal barrier). No-op on non-OpenGL. */
    virtual void MemoryBarrierStorage() {}
    /** After `CopyBuffer` (or GPU read) from staging, before the next CPU `Map`/`Update` on the same buffer. Vulkan: TRANSFER_READ → HOST_WRITE. */
    virtual void BarrierBufferTransferReadToHostWrite(IBuffer* buffer) { (void)buffer; }
    /** After CPU writes to a host-visible buffer (`Map`/`Update`), before `CopyBuffer` from it as source. Vulkan: HOST_WRITE → TRANSFER_READ. */
    virtual void BarrierHostVisibleBufferToTransferRead(IBuffer* buffer) { (void)buffer; }
    /** Bind a pixel-pack buffer (same GL buffer) as SSBO at slot for compute write. pboHandle from CreatePixelPackBuffer. No-op on non-OpenGL. */
    virtual void BindPixelPackBufferAsSSBO(void* pboHandle, uint32_t slot, size_t size) { (void)pboHandle; (void)slot; (void)size; }

    virtual void BindPipeline(IPipeline* pipeline) = 0;
    virtual void BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset = 0) = 0;
    virtual void BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset = 0) = 0;

    /** Draw a fullscreen quad (3 verts or 6 indices) with the given pipeline. Uses device-owned fullscreen VB/IB; pipeline's vertex shader must use layout(location=0) vec2 position, layout(location=1) vec2 texCoord. Single unified path for post, lighting, IBL BRDF, etc.
     */
    virtual void DrawFullscreenQuad(IPipeline* pipeline) = 0;
    virtual void BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset = 0, uint32_t size = 0) = 0;
    virtual void BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset = 0, uint32_t size = 0) = 0;
    /** Bind buffer for atomic counter access (ARB_shader_atomic_counters). Gated by supportsAtomicCounters. */
    virtual void BindAtomicCounterBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) = 0;
    virtual void BindTexture(ITexture* texture, uint32_t binding) = 0;
    virtual void UnbindTexture(uint32_t binding) = 0;
    /** Bind a descriptor set (all textures/buffers updated on it). Pipeline must be bound first; set layout must match pipeline. */
    virtual void BindDescriptorSet(IDescriptorSet* set) = 0;
    /** Bind descriptor sets. firstSet = first set index; count = number of sets; sets = array. dynamicOffsets/dynamicOffsetCount for dynamic UBO/buffer offsets (optional). Pipeline must be bound. No-op if count==0 or sets==nullptr. */
    virtual void BindDescriptorSets(uint32_t firstSet, uint32_t count, IDescriptorSet* const* sets, const uint32_t* dynamicOffsets = nullptr, uint32_t dynamicOffsetCount = 0) { (void)firstSet; (void)count; (void)sets; (void)dynamicOffsets; (void)dynamicOffsetCount; }
    /**
     * Bind only the bindless heap descriptor set (Vulkan: valid only for Forward/GBuffer pipeline layouts; set index is layout-specific).
     * Prefer BindDescriptorSets with the full resource-space set list when possible.
     */
    virtual void BindBindlessHeap(IBindlessHeap* heap) = 0;

    /** Push constants: small per-draw data. See RHIUniformLayout.h for layout. Size must be <= kPushConstantMaxSize. */
    virtual void SetPushConstants(uint32_t offset, uint32_t size, const void* data) = 0;

    virtual void SetUniformInt(const char* name, int value) = 0;
    virtual void SetUniformUint(const char* name, uint32_t value) = 0;
    virtual void SetUniformFloat(const char* name, float value) = 0;
    virtual void SetUniformVec2(const char* name, const float* data) = 0;
    virtual void SetUniformVec3(const char* name, const float* data) = 0;
    virtual void SetUniformVec4(const char* name, const float* data) = 0;
    virtual void SetUniformMat4(const char* name, const float* data) = 0;
    virtual void SetUniformMat4Array(const char* name, const float* data, size_t count) = 0;
    /** Set 64-bit bindless sampler handle (e.g. ARB_bindless_texture). No-op if bindless not supported. */
    virtual void SetUniformHandle64(const char* name, uint64_t handle) = 0;
    /** Set shader subroutine indices for a stage (ARB_shader_subroutine). Pipeline must be bound. Gated by supportsShaderSubroutines. */
    virtual void SetSubroutineIndices(ShaderStage stage, uint32_t count, const uint32_t* indices) = 0;

    virtual void Draw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0) = 0;
    virtual void DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) = 0;
    /** Draw indexed with instance count (e.g. 2 for stereo: one draw, both eyes). */
    virtual void DrawIndexedInstanced(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t instanceCount) = 0;
    /** Single indirect draw (arrays). Buffer must contain DrawArraysIndirectCommand at offset. Gated by caps. */
    virtual void DrawArraysIndirect(IBuffer* indirectBuffer, size_t offset) = 0;
    /** Single indirect draw (indexed). Index buffer must be bound. Buffer contains DrawIndexedIndirectCommand at offset. */
    virtual void DrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset) = 0;
    /** Multi-draw indirect (arrays). drawCount commands at stride bytes apart. Gated by supportsMultiDrawIndirect. */
    virtual void MultiDrawArraysIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) = 0;
    /** Multi-draw indirect (indexed). Index buffer must be bound. Gated by supportsMultiDrawIndirect. */
    virtual void MultiDrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) = 0;
    /** Multi-draw indirect (arrays) with draw count read from countBuffer at countBufferOffset (one uint32_t). Gated by supportsMultiDrawIndirectCount. */
    virtual void MultiDrawArraysIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) = 0;
    /** Multi-draw indirect (indexed) with draw count from countBuffer. Index buffer must be bound. Gated by supportsMultiDrawIndirectCount. */
    virtual void MultiDrawIndexedIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) = 0;
    virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
    /** Dispatch compute with group counts read from indirectBuffer at offset (DispatchIndirectCommand). Same pipeline requirements as Dispatch. Gated by supportsCompute. */
    virtual void DispatchIndirect(IBuffer* indirectBuffer, size_t offset) = 0;

    /** Vulkan `VK_EXT_mesh_shader`: dispatch mesh task groups (`vkCmdDrawMeshTasksEXT`). Requires active dynamic rendering, mesh pipeline bound, `RHICapabilities::supportsMeshShader`. */
    virtual void DrawMeshTasks(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
    /** Vulkan: `vkCmdDrawMeshTasksIndirectEXT` — buffer holds `VkDrawMeshTasksIndirectCommandEXT` records. */
    virtual void DrawMeshTasksIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) = 0;

    /** GPU capture regions (Vulkan: VK_EXT_debug_utils). Default no-op on backends without labels. */
    virtual void PushDebugUtilsLabel(const char* name, const float* colorRgba = nullptr) { (void)name; (void)colorRgba; }
    virtual void PopDebugUtilsLabel() {}
    /** Vulkan `VK_KHR_fragment_shading_rate`: sets per-draw fragment shading rate and combiner ops for subsequent draws in this command buffer. */
    virtual void SetFragmentShadingRate(
        FragmentShadingRate rate,
        FragmentShadingRateCombinerOp pipelinePrimitiveOp = FragmentShadingRateCombinerOp::Keep,
        FragmentShadingRateCombinerOp primitiveAttachmentOp = FragmentShadingRateCombinerOp::Keep) {
        (void)rate;
        (void)pipelinePrimitiveOp;
        (void)primitiveAttachmentOp;
    }

    /** Vulkan: `vkCmdResetQueryPool` for [firstQuery, firstQuery+queryCount), or host `vkResetQueryPool` when `supportsHostQueryReset` while inside dynamic rendering. Cmd reset must be outside active dynamic rendering. No-op default. */
    virtual void ResetQueryPool(IQueryPool* pool, uint32_t firstQuery, uint32_t queryCount) {
        (void)pool;
        (void)firstQuery;
        (void)queryCount;
    }
    virtual void BeginQuery(IQueryPool* pool, uint32_t index) = 0;
    virtual void EndQuery(IQueryPool* pool, uint32_t index) = 0;

    /** Transform feedback: bind buffer for capture slot (0..max TF buffers). Gated by supportsTransformFeedback. */
    virtual void BindTransformFeedbackBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) = 0;
    /** Start capturing vertex/geometry outputs to TF buffers. primitiveMode = output type (Points/Lines/Triangles). */
    virtual void BeginTransformFeedback(PrimitiveTopology primitiveMode) = 0;
    virtual void EndTransformFeedback() = 0;

    /** Conditional render: next draw/dispatch is skipped if query result is 0. Gated by supportsConditionalRender. */
    virtual void BeginConditionalRender(IQueryPool* pool, uint32_t index) = 0;
    virtual void EndConditionalRender() = 0;

    /** Insert a fence at the current point in the command stream. Returns fence to wait on later. No-op returns nullptr. */
    virtual IFence* InsertFence() = 0;

    virtual void CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) = 0;

    /**
     * Vulkan `VK_KHR_acceleration_structure`: record `vkCmdBuildAccelerationStructuresKHR`.
     * `pBuildGeometryInfos` points to `infoCount` `VkAccelerationStructureBuildGeometryInfoKHR` in device build mode.
     * `ppBuildRangeInfos` matches Vulkan’s `const VkAccelerationStructureBuildRangeInfoKHR* const*` (one pointer per info).
     * No-op when `!GetCapabilities().supportsAccelerationStructureBuild` or not recording.
     */
    virtual void BuildAccelerationStructures(uint32_t infoCount, const void* pBuildGeometryInfos, const void* const* ppBuildRangeInfos) {
        (void)infoCount;
        (void)pBuildGeometryInfos;
        (void)ppBuildRangeInfos;
    }
    /**
     * Conservative dependency between AS build writes and subsequent AS reads (e.g. ray query in compute).
     * Vulkan: `VkMemoryBarrier2` with acceleration-structure access/stages. No-op when unsupported.
     */
    virtual void MemoryBarrierAccelerationStructureAccess() {}
};

// ---- Fence (CPU wait for GPU) ----

struct IFence {
    virtual ~IFence() = default;
};

// ---- Buffer ----

struct IBuffer {
    virtual ~IBuffer() = default;
    virtual size_t GetSize() const = 0;
    virtual void* Map(size_t offset = 0, size_t size = 0) = 0;  // 0 size = whole buffer
    virtual void Unmap() = 0;
    virtual void Update(const void* data, size_t offset, size_t size) = 0;
};

struct IAccelerationStructure {
    virtual ~IAccelerationStructure() = default;
    virtual AccelerationStructureType GetAccelerationStructureType() const = 0;
    /** Vulkan `vkGetAccelerationStructureDeviceAddressKHR` when ray-query slice is enabled; else 0. */
    virtual uint64_t GetDeviceAddress() const = 0;
    virtual uint64_t GetSizeInBytes() const = 0;
    /** Vulkan: `VkAccelerationStructureKHR` as `void*`; nullptr when not applicable. */
    virtual void* GetNativeAccelerationStructure() { return nullptr; }
};

// ---- Texture ----

struct ITexture {
    virtual ~ITexture() = default;
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual TextureType GetType() const = 0;
    virtual RHITextureFormat GetFormat() const = 0;
    /** Backend-native texture handle (e.g. GL texture id as void*). For interop only. */
    virtual void* GetNativeTexture() = 0;
    /**
     * Vulkan: for Texture2DArray (and similar), GetNativeTexture() is often a 2D array image view.
     * Code that binds into a sampler2D / Dim=2D path must use a single-layer 2D view instead.
     * Returns nullptr if unsupported or layer out of range (callers use GetNativeTexture()).
     */
    virtual void* GetNativeTexture2DViewForArrayLayer(uint32_t layer) { (void)layer; return nullptr; }
    /** False after GPU resources are torn down (e.g. Vulkan deferred destroy with live wrapper). */
    virtual bool IsGpuBindable() const { return true; }
};

// ---- Shader ----

struct IShader {
    virtual ~IShader() = default;
    virtual ShaderStage GetStage() const = 0;
    virtual bool IsValid() const = 0;
};

// ---- Pipeline layout ----

struct IPipelineLayout {
    virtual ~IPipelineLayout() = default;
    virtual const PipelineLayoutDesc& GetDesc() const = 0;
};

// ---- Bindless heap ----

struct IBindlessHeap {
    virtual ~IBindlessHeap() = default;
    /** Register texture and get a bindless handle for use in shaders. Returns kInvalidBindlessHandle if unsupported or failed. */
    virtual BindlessHandle AllocateTextureHandle(ITexture* texture) = 0;
    /** Register buffer and get a bindless handle. Returns kInvalidBindlessHandle if unsupported or failed. */
    virtual BindlessHandle AllocateBufferHandle(IBuffer* buffer) = 0;
    /** Release a texture handle (optional; heap may be fixed-size and not support free). */
    virtual void FreeTextureHandle(BindlessHandle handle) = 0;
    /** Release a buffer handle (optional). */
    virtual void FreeBufferHandle(BindlessHandle handle) = 0;
    /** Descriptor set for resource-space set 3 (bindless). Returns nullptr if unsupported. Do not destroy the returned set. */
    virtual IDescriptorSet* GetDescriptorSet() { return nullptr; }
};

// ---- Descriptor set ----

struct IDescriptorSet {
    virtual ~IDescriptorSet() = default;
    virtual void UpdateTexture(uint32_t binding, ITexture* texture) = 0;
    virtual void UpdateUniformBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset = 0, uint32_t size = 0) = 0;
    virtual void UpdateStorageBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset = 0, uint32_t size = 0) = 0;
};

// ---- Pipeline ----

/** Compute pipeline SSBO block presence (e.g. cull shader: IndirectOut at 0, DrawListIn at 1). Set at pipeline create; default false. */
struct ComputeBlockPresence {
    bool hasIndirectOut = false;
    bool hasDrawListIn = false;
};

struct IPipeline {
    virtual ~IPipeline() = default;
    /** False when pipeline was created but has no valid backend program. */
    virtual bool HasValidProgram() const { return true; }
    /** Compute only: which storage blocks exist. Default both false; backends set at CreateComputePipeline. */
    virtual void GetComputeBlockPresence(ComputeBlockPresence& out) const {
        out.hasIndirectOut = false;
        out.hasDrawListIn = false;
    }
    /** True when pipeline uses Vulkan mesh shading (`DrawMeshTasks` / `DrawMeshTasksIndirect` instead of `Draw`). */
    virtual bool UsesMeshShaders() const { return false; }
};

// ---- Render target ----

struct IRenderTarget {
    virtual ~IRenderTarget() = default;
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual ITexture* GetColorTexture(uint32_t index) = 0;
    virtual ITexture* GetDepthTexture() = 0;
    /** Backend-native framebuffer handle (e.g. GL FBO id as void*). For interop only. */
    virtual void* GetNativeFramebuffer() = 0;
    /** For depth-only array RTs: set layer to render to (0..arrayLayers-1). No-op for non-array. */
    virtual void SetDepthLayer(uint32_t layer) { (void)layer; }
    virtual uint32_t GetDepthLayer() const { return 0; }
};

// ---- Query pool ----

struct IQueryPool {
    virtual ~IQueryPool() = default;
    virtual QueryType GetType() const = 0;
    virtual uint32_t GetCount() const = 0;
    /** Timestamp: nanoseconds after **`VkPhysicalDeviceLimits::timestampPeriod`** scaling in **`QueryPoolVK::GetResult`**
     *  (`BeginQuery(i)` / `EndQuery(i)` use **`vkCmdWriteTimestamp`** at **`i`** and **`i+1`**). Occlusion: samples passed (**uint64**).
     *  Pipeline statistics: use **`GetPipelineStatistics`** (**`GetResult`** returns 0). See **`eyelib/rendering/rhi/Vulkan/query_pool_dynamic_rendering.md`**. */
    virtual uint64_t GetResult(uint32_t index) = 0;
    /** For `QueryType::PipelineStatistics`: number of `uint64_t` counters per query (popcount of pool mask). Else 0. */
    virtual uint32_t GetPipelineStatisticsSlotCount() const { return 0; }
    /** For `QueryType::PipelineStatistics`: writes up to `min(maxSlots, GetPipelineStatisticsSlotCount())` words for `queryIndex`. Returns words written, or 0. */
    virtual uint32_t GetPipelineStatistics(uint32_t queryIndex, uint64_t* outSlots, uint32_t maxSlots) {
        (void)queryIndex;
        (void)outSlots;
        (void)maxSlots;
        return 0;
    }
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
