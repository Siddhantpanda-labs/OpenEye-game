#pragma once

#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/Vulkan/commands/CommandListVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/FrameContextVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/SwapchainVK.h"
#include "eyelib/rendering/rhi/Vulkan/memory/DeletionQueueVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/QueuesVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/InstanceVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/PhysicalDeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/memory/AllocatorVK.h"
#include "eyelib/rendering/rhi/Vulkan/memory/UploadHeapVK.h"
#include "eyelib/rendering/rhi/Vulkan/descriptors/DescriptorAllocatorVK.h"
#include "eyelib/rendering/rhi/Vulkan/descriptors/DescriptorCacheVK.h"
#include "eyelib/rendering/rhi/Vulkan/barriers/BarrierTrackerVK.h"
#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

struct BufferVK;
struct TextureVK;
struct ShaderVK;
struct PipelineVK;
struct PipelineLayoutVK;
struct DescriptorSetVK;
struct RenderTargetVK;
struct QueryPoolVK;
struct FenceVK;
struct BindlessHeapVK;
struct AccelerationStructureVK;

/** Vulkan RHI device. Handles are opaque (void*) to avoid requiring vulkan.h in this header.
 *  Queue policy: swapchain **present** and main frame work use the **graphics** queue; see `QueuesVK`.
 *
 * **Threading:** Intended use is a **single render thread** for `Initialize`, resource create or destroy,
 *  `BeginFrame` / `EndFrame`, recording `ICommandList`, and present. Do not record the same command list from
 *  multiple threads; deferred lists follow the submit and free path documented on `SubmitCommandList` / `DestroyCommandList`.
 *
 * **Lifetime:** The instance, device, surface, and swapchain are owned by `DeviceVK` for its lifetime (destructor tears down).
 *  Do not use `ITexture`, `IBuffer`, or other RHI objects after the matching `Destroy*` call. Swapchain image indices and
 *  views are only valid until the next `SwapchainVK::Resize` or device teardown. */
struct DeviceVK : IDevice {
    DeviceVK();
    ~DeviceVK() override;

    const RHICapabilities& GetCapabilities() const override;
    bool IsDeviceLost() const override { return m_deviceLost; }
    ::eyelib::rendering::RenderBackend GetRenderBackend() const override { return ::eyelib::rendering::RenderBackend::Vulkan; }
    const char* GetShaderBackendName() const override;
    const char* GetShaderVertexExtension() const override;
    const char* GetShaderFragmentExtension() const override;
    bool ResolveShaderPath(const char* logicalName, ShaderStage stage, std::string& outPath) override;

    ICommandList* CreateCommandList() override;
    void DestroyCommandList(ICommandList* list) override;
    ICommandList* GetImmediateCommandList() override;
    ICommandList* GetTransientRecordingCommandList() const override;
    void PushTransientRecordingCommandList(ICommandList* cmd) override;
    void PopTransientRecordingCommandList(ICommandList* cmd) override;
    void EnqueueFrameSliceCommandList(ICommandList* cmd) override;
    void SubmitCommandList(ICommandList* list) override;
    void SetAsyncComputeSubmitPolicy(AsyncComputeSubmitPolicy policy) override;
    AsyncComputeSubmitPolicy GetAsyncComputeSubmitPolicy() const override;

    void* GetBarrierTracker() override;
    void TransitionTextureToState(ITexture* texture, ResourceState newState) override;
    ResourceState GetTextureTrackedState(const ITexture* texture) const override;
    void FlushBarrierTracker(ICommandList* cmd) override;

    IBuffer* CreateBuffer(const BufferDesc& desc, const void* initialData) override;
    void DestroyBuffer(IBuffer* buffer) override;
    uint64_t GetBufferDeviceAddress(IBuffer* buffer) override;

    bool GetAccelerationStructureBuildSizes(const AccelerationStructureBuildSizesInput& input, AccelerationStructureBuildSizes& outSizes) override;
    IAccelerationStructure* CreateAccelerationStructure(const AccelerationStructureDesc& desc) override;
    void DestroyAccelerationStructure(IAccelerationStructure* as) override;
    IAccelerationStructureScratchBuffer* CreateAccelerationStructureScratchBuffer(const AccelerationStructureScratchBufferDesc& desc,
                                                                                   const void* initialData) override;
    IAccelerationStructureInstanceBuffer* CreateAccelerationStructureInstanceBuffer(const AccelerationStructureInstanceBufferDesc& desc,
                                                                                    const void* initialData) override;

    ITexture* CreateTexture(const TextureDesc& desc, const void* initialData) override;
    void DestroyTexture(ITexture* texture) override;
    void CommitSparseTexturePages(ITexture* texture, uint32_t level, int32_t x, int32_t y, int32_t z, uint32_t width, uint32_t height, uint32_t depth, bool commit) override;
    void CommitSparseBufferPages(IBuffer* buffer, size_t offset, size_t size, bool commit) override;
    ITexture* CreateTextureView(ITexture* source, RHITextureFormat viewFormat, uint32_t baseMipLevel, uint32_t mipLevels, uint32_t baseLayer, uint32_t layerCount) override;
    ITexture* CreateTextureBuffer(IBuffer* buffer, RHITextureFormat format, size_t offset, size_t size) override;
    bool WaitFence(IFence* fence, uint64_t timeoutMs) override;
    void DestroyFence(IFence* fence) override;

    IShader* CreateShader(ShaderStage stage, const char* source, size_t sourceLen) override;
    IShader* CreateShaderFromBinary(ShaderStage stage, const void* data, size_t size) override;
    bool TryLoadShaderFromPaths(const char* vertexPath, const char* fragmentPath, IShader*& outVs, IShader*& outFs) override;
    void DestroyShader(IShader* shader) override;

    IPipeline* CreateGraphicsPipeline(const PipelineDesc& desc) override;
    IPipeline* CreateGraphicsPipelineFromShaderAsset(const ShaderGraphicsPipelineCreateInfo& info) override;
    IPipeline* CreateGraphicsPipelineFromBinary(const void* data, size_t size, uint32_t binaryFormat, const VertexLayout& layout, PrimitiveTopology topology) override;
    bool GetPipelineBinary(IPipeline* pipeline, std::vector<uint8_t>& outData, uint32_t& outFormat) override;
    IPipeline* CreateComputePipeline(void* computeShader) override;
    IPipeline* CreateComputePipelineWithLayout(void* computeShader, PipelineLayoutKind layoutKind) override;
    void DestroyPipeline(IPipeline* pipeline) override;
    void EnsureComputePipelineStorageBlockBindings(IPipeline* pipeline) override;
    bool ComputePipelineHasStorageBlock(IPipeline* pipeline, const char* blockName) override;

    IRenderTarget* CreateRenderTarget(uint32_t width, uint32_t height,
                                      const std::vector<RHITextureFormat>& colorFormats,
                                      RHITextureFormat depthFormat,
                                      uint32_t samples = 1,
                                      bool transient = false) override;
    IRenderTarget* CreateDepthOnlyRenderTargetArray(uint32_t width, uint32_t height, uint32_t arrayLayers) override;
    IRenderTarget* CreateRenderTargetWithSharedDepth(uint32_t width, uint32_t height,
                                                     const std::vector<RHITextureFormat>& colorFormats,
                                                     ITexture* sharedDepthTexture,
                                                     uint32_t samples) override;
    IRenderTarget* CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) override;
    IRenderTarget* CreateRenderTargetFromTextureWithDepth(ITexture* colorTexture, uint32_t face, uint32_t mipLevel, ITexture* depthTexture) override;
    void DestroyRenderTarget(IRenderTarget* rt) override;

    IRenderTarget* GetSharedShadowMapArrayDummyRenderTarget() override;
    void EnsureSharedShadowMapArrayDummyForShaderSampling(ICommandList* cmd) override;

    ITexture* CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) override;

    IQueryPool* CreateQueryPool(QueryType type, uint32_t count, PipelineStatisticsFlags pipelineStatisticsMask = 0) override;
    void DestroyQueryPool(IQueryPool* pool) override;
    void ResetQueryPoolOnHost(IQueryPool* pool, uint32_t firstQuery, uint32_t queryCount) override;

    IPipelineLayout* CreatePipelineLayout(const PipelineLayoutDesc& desc) override;
    void DestroyPipelineLayout(IPipelineLayout* layout) override;
    IDescriptorSet* CreateDescriptorSet(IPipelineLayout* layout) override;
    void DestroyDescriptorSet(IDescriptorSet* set) override;
    IPipelineLayout* GetResourceSetLayout(ResourceSetType type) override;
    IDescriptorSet* GetEmptyResourceSet() override { return m_emptyResourceSet; }

    IBindlessHeap* CreateBindlessHeap() override;
    void DestroyBindlessHeap(IBindlessHeap* heap) override;
    IBindlessHeap* GetOrCreateSharedBindlessHeap() override;
    BindlessHandle GetOrAllocateSharedBindlessTextureHandle(ITexture* texture) override;

    int GetMaxSamples(int requestedSamples) override;

    /** Frame ring (`kMaxFrames`): BeginFrame waits this slot’s fence, ticks deferred deletion, resets pool/fence, acquires swapchain image; EndFrame submits command buffers with that fence and (if presenting) waits `imageAvailable` and signals `renderFinished`. Present runs on the graphics queue.
     *
     * Frame/submit invariants (Phase 4.1):
     * - Timeline semaphore (`m_timelineSemaphore`) orders deferred submits and cross-queue bridge work; it is distinct from per-frame binary semaphores (`imageAvailable`/`renderFinished`) used for acquire/present.
     * - Deferred compute submits may route to compute queue only when eligibility checks pass; otherwise they submit on graphics queue with the same timeline ordering contract.
     * - `WaitForPresent`/`WaitForPreviousPresent` are opt-in pacing helpers (`VK_KHR_present_wait`) and do not change queue ownership or implicit submission ordering; see `swapchain_present_policy.md` §4.
     * - Upload helper submits (`TextureVK`/`BufferVK`) currently use graphics-family-safe command pools and queue selection policy; dedicated transfer-family ownership handoff is intentionally not implicit. */
    void BeginFrame() override;
    void EndFrame() override;
    GpuFramePhase GetGpuFramePhase() const override { return m_gpuFramePhase; }
    void Present() override;
    void OnWindowResized(uint32_t width, uint32_t height) override;
    void SetSwapchainPresentMode(SwapchainPresentMode mode) override;
    SwapchainPresentMode GetSwapchainPresentMode() const override { return m_swapchainPresentMode; }
    void SetSwapchainOutputMode(SwapchainOutputMode mode) override;
    SwapchainOutputMode GetSwapchainOutputMode() const override { return m_swapchainOutputMode; }
    void GetSwapchainSurfaceColorInfo(uint32_t* outVkFormat, uint32_t* outVkColorSpaceKHR) const override;
    bool WaitForPresent(uint64_t presentId, uint64_t timeoutNs = UINT64_MAX) override;
    bool WaitForPreviousPresent(uint64_t timeoutNs = UINT64_MAX) override;
    uint64_t GetLastPresentId() const override;
    /** True when `VK_EXT_swapchain_colorspace` was enabled at device create (HDR/wide-gamut surface pairs). */
    bool SupportsSwapchainColorspace() const { return m_supportsSwapchainColorspace; }
    /** True when `VK_KHR_present_id` + `VK_KHR_present_wait` are enabled (`vkWaitForPresentKHR`). */
    bool SupportsPresentWait() const { return m_supportsPresentWait; }
    /** True when `VK_EXT_descriptor_buffer` is enabled (`VkPhysicalDeviceDescriptorBufferFeaturesEXT::descriptorBuffer`). */
    bool SupportsDescriptorBuffer() const { return m_supportsDescriptorBuffer; }
    /** True when `VK_EXT_graphics_pipeline_library` is enabled (`VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT::graphicsPipelineLibrary`). */
    bool SupportsGraphicsPipelineLibrary() const { return m_supportsGraphicsPipelineLibrary; }
    /** When `VK_KHR_pipeline_executable_properties` is enabled: one **Info** line with executable count + first ISA name (no-op otherwise). Safe to call after every pipeline create; runs at most once per device. */
    void DeveloperLogPipelineExecutablePropertiesOnce(void* vkPipeline, const char* debugName);
    void Finish() override;
    void BlitToDefaultFramebuffer(IRenderTarget* src) override;
    void BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) override;
    void BlitToDefaultFramebufferSubregion(IRenderTarget* src, int dstX, int dstY, int dstW, int dstH) override;
    void BlitToDefaultFramebufferLetterbox(IRenderTarget* src, uint32_t windowWidth, uint32_t windowHeight) override;
    void SetPostBlitCallback(std::function<void(void*)> cb) override;
    bool GetImGuiVulkanInitInfo(ImGuiVulkanInitInfo* out) override;
    void Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) override;
    void BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) override;
    void BlitDepthOnly(IRenderTarget* src, IRenderTarget* dst) override;
    bool ReadPixelUint32(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t* outValue) override;
    bool ReadPixelsRGBA8(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t* outBuffer) override;
    void* CreatePixelPackBuffer(size_t sizeInBytes) override;
    void DestroyPixelPackBuffer(void* handle) override;
    void ReadPixelsToPBO(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, void* pboHandle) override;
    void MapPixelPackBuffer(void* pboHandle, void* outBuffer, size_t sizeInBytes, uint32_t width, uint32_t height) override;
    void MapPixelPackBufferRaw(void* pboHandle, void* outBuffer, size_t sizeInBytes) override;

    /** Initialize Vulkan instance and device. Called by CreateVulkanDevice. Returns false on failure. */
    bool Initialize(void* windowHandle, uint32_t width, uint32_t height);
    /** Sets `m_deviceLost` and logs once per `VK_ERROR_DEVICE_LOST`; when `VK_EXT_device_fault` is enabled, best-effort `vkGetDeviceFaultInfoEXT` detail (Phase 12). */
    void NoteVulkanDeviceLost(const char* context, int vkResult);
    /** Opaque Vulkan instance (VkInstance). */
    void* GetInstance() const { return m_instanceVK.GetHandle(); }
    /** Opaque Vulkan device (VkDevice). */
    void* GetDevice() const { return m_device; }
    /** Opaque Vulkan physical device (VkPhysicalDevice). */
    void* GetPhysicalDevice() const { return m_physicalDeviceVK.GetHandle(); }
    /** Opaque VMA allocator (VmaAllocator). */
    void* GetAllocator() const { return m_allocatorVK.GetHandle(); }
    /** Return graphics queue family index (for command pool). */
    uint32_t GetGraphicsQueueFamily() const { return m_queues.GetGraphicsQueueFamily(); }
    /** Free a command buffer back to its pool (deferred lists; pool was stored when allocating from frame). */
    void FreeCommandBuffer(void* cmdBuf, void* commandPool);
    /** Opaque graphics queue (VkQueue). */
    void* GetGraphicsQueue() const { return m_queues.GetGraphicsQueue(); }
    /** Opaque transfer queue (VkQueue). May alias graphics queue on single-family devices. */
    void* GetTransferQueue() const { return m_queues.GetTransferQueue(); }
    /** Transfer queue family index. May equal graphics family. */
    uint32_t GetTransferQueueFamily() const { return m_queues.GetTransferQueueFamily(); }
    /** Opaque descriptor pool (VkDescriptorPool). */
    void* GetDescriptorPool() const { return m_descriptorPool; }
    /** Opaque default linear sampler (VkSampler) for color textures. */
    void* GetDefaultSampler() const { return m_defaultSampler; }
    /** Opaque nearest sampler (VkSampler), used for integer/ID textures. Falls back to default sampler when unavailable. */
    void* GetNearestSampler() const { return m_nearestSampler ? m_nearestSampler : m_defaultSampler; }
    /** Opaque pipeline cache (VkPipelineCache) for pipeline creation. */
    void* GetPipelineCache() const { return m_pipelineCache; }
    /** True when VK_EXT_extended_dynamic_state is enabled on the device. */
    bool SupportsExtendedDynamicState() const { return m_supportsExtendedDynamicState; }
    /** True when synchronization2 barriers are enabled (`vkCmdPipelineBarrier2`). */
    bool SupportsSynchronization2() const { return m_supportsSynchronization2; }
    /** True when buffer device address feature is enabled (`vkGetBufferDeviceAddress`). */
    bool SupportsBufferDeviceAddress() const { return m_supportsBufferDeviceAddress; }
    /** Descriptor-buffer encoded sizes from `VkPhysicalDeviceDescriptorBufferPropertiesEXT` (0 when unavailable). */
    uint32_t DescriptorBufferCombinedImageSamplerDescriptorSize() const { return m_descriptorBufferCombinedImgSize; }
    uint32_t DescriptorBufferStorageBufferDescriptorSize() const { return m_descriptorBufferStorageBufSize; }
    uint64_t DescriptorBufferOffsetAlignment() const { return m_descriptorBufferOffsetAlignment; }
    void* DescriptorBufferFnGetDescriptorEXT() const { return m_pfnGetDescriptorEXT; }
    void* DescriptorBufferFnGetSetLayoutSizeEXT() const { return m_pfnGetDescriptorSetLayoutSizeEXT; }
    void* DescriptorBufferFnGetBindingOffsetEXT() const { return m_pfnGetDescriptorSetLayoutBindingOffsetEXT; }
    void* DescriptorBufferFnCmdBindBuffersEXT() const { return m_pfnCmdBindDescriptorBuffersEXT; }
    void* DescriptorBufferFnCmdSetOffsetsEXT() const { return m_pfnCmdSetDescriptorBufferOffsetsEXT; }
    /** Default pipeline layout: PushOnly (0 sets, push constants only). Used for graphics pipelines without a resource-space layout (e.g. BRDF LUT, fullscreen quads). */
    void* GetDefaultPipelineLayout() const { return m_defaultPipelineLayout; }
    /** PushOnly layout with task|mesh|fragment push-constant stages; for `PipelineDesc::meshShader` + `PipelineLayoutKind::PushOnly`. */
    void* GetMeshPushPipelineLayout() const { return m_meshPushPipelineLayout; }
    /** True when `VK_EXT_mesh_shader` was enabled and `vkCmdDrawMeshTasksEXT` loaded. */
    bool SupportsMeshShader() const { return m_supportsMeshShader; }
    /** True when `VK_KHR_fragment_shading_rate` is enabled and `vkCmdSetFragmentShadingRateKHR` loaded. */
    bool SupportsVariableRateShading() const { return m_supportsVariableRateShading; }
    /** True when Vulkan acceleration-structure + ray-query slice is enabled (`VK_KHR_acceleration_structure` + `VK_KHR_ray_query`). */
    bool SupportsRayQuery() const { return m_supportsRayQuery; }
    /** From `VkPhysicalDeviceAccelerationStructurePropertiesKHR::minAccelerationStructureScratchOffsetAlignment` (1 when unknown). */
    uint32_t GetRayTracingScratchAlignment() const { return m_rayTracingScratchAlignment ? m_rayTracingScratchAlignment : 1u; }
    void* GetCmdDrawMeshTasksEXT() const { return m_pfnCmdDrawMeshTasksEXT; }
    void* GetCmdDrawMeshTasksIndirectEXT() const { return m_pfnCmdDrawMeshTasksIndirectEXT; }
    void* GetCmdSetFragmentShadingRateKHR() const { return m_pfnCmdSetFragmentShadingRateKHR; }
    void* GetCmdBuildAccelerationStructuresKHR() const { return m_pfnCmdBuildAccelerationStructuresKHR; }
    void* GetCreateAccelerationStructureKHR() const { return m_pfnCreateAccelerationStructureKHR; }
    void* GetDestroyAccelerationStructureKHR() const { return m_pfnDestroyAccelerationStructureKHR; }
    void* GetGetAccelerationStructureBuildSizesKHR() const { return m_pfnGetAccelerationStructureBuildSizesKHR; }
    void* GetGetAccelerationStructureDeviceAddressKHR() const { return m_pfnGetAccelerationStructureDeviceAddressKHR; }
    /** Refcount `VkBuffer` handles that currently host at least one `VkAccelerationStructureKHR` (Phase 11.5). */
    void NoteAccelerationStructureCreatedOnBuffer(void* vkBuffer);
    void NoteAccelerationStructureDestroyedForBuffer(void* vkBuffer);
    /** Bindless descriptor set layout (set 1: textures binding 0, buffers binding 1). */
    void* GetBindlessSetLayout() const { return m_bindlessSetLayout; }
    /** IPipelineLayout wrapper for bindless set layout (for use with CreateDescriptorSetWrapperVK). */
    IPipelineLayout* GetBindlessPipelineLayout() const;
    /** Empty bindless set to bind at set 1 when no heap is bound (partially bound). */
    void* GetEmptyBindlessSet() const { return m_emptyBindlessSet; }
    /** Compute set 0 (bindings 0,1,2,3 storage; 4 image; 5 UBO). Updated by UpdateComputeSet0(cmd). */
    void* GetComputeSet0() const { return m_computeSet0; }
    void* GetComputePipelineLayout() const { return m_computePipelineLayout; }
    /** Update compute set 0 from command list bound buffers/textures/UBOs. Call before compute dispatch. */
    void UpdateComputeSet0(CommandListVK* cmd);
    /** Pipeline layout for resource-space kind (Forward, Post, Shadow, IBL, ComputeLightCull, ComputeGpuCull). Returns nullptr for PushOnly/Legacy — use GetDefaultPipelineLayout or GetComputePipelineLayout for compute. */
    void* GetPipelineLayoutForKind(PipelineLayoutKind kind) const;
    /** Legacy compute params UBO (binding 1; ParamsBlock for light_cull). Host-visible; update before dispatch. */
    IBuffer* GetLegacyComputeParamsUBO() const { return m_legacyComputeParamsUBO; }
    /** Timeline semaphore for GPU completion (set 1 = graphics). Used by InsertFence/Submit/WaitFence. */
    void* GetTimelineSemaphore() const { return m_timelineSemaphore; }
    /** Reserve next timeline value for a fence; signal at submit. Thread-safe only if single-threaded submit. */
    uint64_t ReserveTimelineValue() { return ++m_timelineValue; }
    /** Look up or register graphics pipeline by hash (used when creating pipelines). Returns existing VkPipeline or VK_NULL_HANDLE. */
    void* GetCachedGraphicsPipeline(uint64_t hash) const;
    /** Register a created graphics pipeline in the cache (refCount 0 until RetainGraphicsPipeline). */
    void RegisterGraphicsPipeline(uint64_t hash, void* pipeline);
    /** Increment refcount for a cached graphics VkPipeline (one per IPipeline wrapper). */
    void RetainGraphicsPipeline(uint64_t hash);
    /** Decrement refcount; when zero, enqueue VkPipeline destruction and remove from cache. */
    void ReleaseGraphicsPipeline(uint64_t hash, void* vkPipeline);
    /** Fullscreen quad VBO/IBO for DrawFullscreenQuad (lazy-created). */
    IBuffer* GetFullscreenQuadVBO() const { return m_fullscreenQuadVBO; }
    IBuffer* GetFullscreenQuadIBO() const { return m_fullscreenQuadIBO; }
    void EnsureFullscreenQuad();
    /** Active list for Blit/BlitDepthOnly: transient slice when set, else immediate. */
    CommandListVK* GetRecordingCommandListVK();
    /** True between BeginFrame() and EndFrame(). CommandListVK uses this to avoid resetting the immediate buffer mid-frame. */
    bool IsFrameOpen() const { return m_frameOpen; }
    /** Current frame index (0 .. kMaxFrames-1). Used by DeletionQueue, DescriptorCache, UploadHeap, etc. */
    uint32_t GetCurrentFrameIndex() const override { return m_currentFrameIndex; }
    /** Monotonic frame number (incremented each BeginFrame). For profiling and frame-based logic. */
    uint32_t GetFrameNumber() const override { return m_frameNumber; }
    void EnqueueDestroyDescriptorSet(IDescriptorSet* set) override;
    /** Deferred destruction queue. Enqueue here instead of destroying immediately; Tick is called in BeginFrame. */
    DeletionQueueVK* GetDeletionQueue() { return &m_deletionQueue; }
    /** Ring staging buffer with per-frame regions. BeginFrame(..., frameIndex) resets that frame's region; Allocate(heap, frameIndex, ...) then vkCmdCopyBuffer from GetBuffer(). */
    UploadHeapVK* GetUploadHeap() { return &m_uploadHeap; }
    /** Per-frame descriptor pools for transient sets. Reset in BeginFrame; allocate with Allocate(device, frameIndex, layout, &set). */
    DescriptorAllocatorVK* GetDescriptorAllocator() { return &m_descriptorAllocator; }
    /** Per-frame descriptor set cache (layoutHash→set per frame). GetOrCreate(device, frameIndex, layout, layoutHash, &set); Reset in BeginFrame. */
    DescriptorCacheVK* GetDescriptorCache() { return &m_descriptorCache; }
    /** Tracks resource state; TransitionBuffer/TransitionImage then FlushBarriers(cmdList). Reset in BeginFrame. */
    BarrierTrackerVK* GetBarrierTrackerVK() { return &m_barrierTracker; }
    /** 1x1 RGBA8 for descriptor writes when a texture is null or not GPU-bindable (2D bindings). */
    ITexture* GetFallbackShaderResourceTexture2D();
    /** 1x1 RGBA8 cubemap for cube/samplerCube bindings in the same situation. */
    ITexture* GetFallbackShaderResourceTextureCube();
    /** `VK_EXT_swapchain_maintenance1`: release all image indices for `vkSwapchain` before `vkDestroySwapchainKHR`. No-op if unsupported. */
    void ReleaseSwapchainImagesForMaintenance(void* vkSwapchain, uint32_t imageCount);

private:
    /** `VK_ERROR_OUT_OF_DATE_KHR` / `VK_SUBOPTIMAL_KHR` handling for acquire + present (see `swapchain_present_policy.md`). */
    enum class SwapchainKhrPath : uint8_t { Acquire, Present };
    void NoteSwapchainKhrResult(const char* context, int vkResult, SwapchainKhrPath path);

    void QueryCapabilities();
    void TryLogDeviceFaultInfoEXT(const char* lossContext);
    /** Precondition: `m_sharedBindlessMutex` is held. Returns nullptr if heap allocation failed. */
    IBindlessHeap* EnsureSharedBindlessHeapLocked();

    RHICapabilities m_caps;
    InstanceVK m_instanceVK;
    PhysicalDeviceVK m_physicalDeviceVK;
    void* m_device = nullptr;
    QueuesVK m_queues;
    static constexpr uint32_t kMaxFrames = 3;
    std::array<FrameContextVK, kMaxFrames> m_frameContexts = {};
    uint32_t m_currentFrameIndex = 0;   // ring index 0..kMaxFrames-1
    uint32_t m_frameNumber = 0;          // monotonic (for DeletionQueue, FrameGraph, etc.)
    void* m_descriptorPool = nullptr;  // VkDescriptorPool
    void* m_defaultSampler = nullptr;  // VkSampler (linear)
    void* m_nearestSampler = nullptr;  // VkSampler (nearest)
    void* m_pipelineCache = nullptr;  // VkPipelineCache
    void* m_defaultPipelineLayout = nullptr;  // VkPipelineLayout (PushOnly: 0 sets, push constants only)
    void* m_meshPushPipelineLayout = nullptr; // VkPipelineLayout (PushOnly, push visible to task|mesh|fragment)
    void* m_bindlessSetLayout = nullptr;      // VkDescriptorSetLayout for bindless heap (set 1)
    mutable IPipelineLayout* m_bindlessPipelineLayoutWrapper = nullptr;  // wrapper for GetBindlessPipelineLayout
    void* m_emptyBindlessSet = nullptr;       // VkDescriptorSet (partially bound, for set 1 when no heap bound)
    void* m_emptyBindlessPool = nullptr;     // VkDescriptorPool for m_emptyBindlessSet
    void* m_computeSet0Layout = nullptr;     // VkDescriptorSetLayout (set 0 compute: 0,2,3 storage; 1,5 UBO; 4 image)
    void* m_computeSet0 = nullptr;           // VkDescriptorSet (updated by UpdateComputeSet0)
    void* m_computePipelineLayout = nullptr; // VkPipelineLayout (compute set0 + bindless set1 + push 16 bytes)
    // Resource-space layouts (set0 Frame, set1 Pass, set2 Material, set3 Bindless, set4 SceneBuffers). See descriptor_binding_table.md.
    void* m_frameSetLayout = nullptr;
    void* m_forwardPassSetLayout = nullptr;
    void* m_postPassSetLayout = nullptr;
    void* m_shadowPassSetLayout = nullptr;
    void* m_lightCullPassSetLayout = nullptr;
    void* m_materialSetLayout = nullptr;
    void* m_sceneBuffersSetLayout = nullptr;
    void* m_deferredLightingPassSetLayout = nullptr;
    void* m_iblPassSetLayout = nullptr;       // VkDescriptorSetLayout (set 0: env cube)
    void* m_iblPipelineLayout = nullptr;      // VkPipelineLayout (IBL: 1 set)
    void* m_forwardPipelineLayout = nullptr;
    void* m_postPipelineLayout = nullptr;
    void* m_shadowPipelineLayout = nullptr;
    void* m_gbufferPipelineLayout = nullptr;
    void* m_deferredLightingPipelineLayout = nullptr;
    void* m_computeLightCullPipelineLayout = nullptr;
    void* m_computeGpuCullPipelineLayout = nullptr;
    void* m_outlinePostSetLayout = nullptr;
    void* m_outlinePostPipelineLayout = nullptr;
    void* m_composeBootstrapPipelineLayout = nullptr;
    void* m_RasterForwardMinimalPipelineLayout = nullptr;
    /** Set1 layout for `PipelineLayoutKind::RasterForwardMinimal` (PerObject SSBO + directional light UBO). */
    void* m_rasterForwardDataSetLayout = nullptr;
    /** Set1 layout for `PipelineLayoutKind::RasterDeferredLighting` (G-Buffer + dir light). */
    void* m_RasterDeferredLightingDataSetLayout = nullptr;
    /** Full pipeline layout for `PipelineLayoutKind::RasterDeferredLighting` (set0 Frame, set1 RasterDeferredLightingData). */
    void* m_RasterDeferredLightingPipelineLayout = nullptr;
    /** Wrappers for GetResourceSetLayout (single-set IPipelineLayout per type); do not own set layouts. */
    IPipelineLayout* m_resourceSetLayouts[12] = {};  // Frame … OutlinePost, RasterDeferredLightingData
    void* m_emptyResourceSetLayout = nullptr;       // VkDescriptorSetLayout (0 bindings, placeholder for compute sets 1–3)
    IPipelineLayout* m_emptyResourceSetLayoutWrapper = nullptr;
    IDescriptorSet* m_emptyResourceSet = nullptr;   // one set from m_emptyResourceSetLayout, for BindDescriptorSets placeholder
    rhi::IBuffer* m_legacyComputeParamsUBO = nullptr; // 256 bytes, binding 1 (ParamsBlock for light_cull)
    void* m_timelineSemaphore = nullptr;     // VkSemaphore (timeline, for fence sync)
    uint64_t m_timelineValue = 0;           // next value to signal on submit
    uint64_t m_lastDeferredSubmitTimelineValue = 0; // preserves deferred submit order across graphics/compute queues
    AsyncComputeSubmitPolicy m_asyncComputeSubmitPolicy = AsyncComputeSubmitPolicy::Automatic;
    void* m_surface = nullptr;              // VkSurfaceKHR
    SwapchainVK m_swapchain;                 // swapchain, images, views, framebuffers, render pass
    SwapchainPresentMode m_swapchainPresentMode = SwapchainPresentMode::Fifo;
    SwapchainOutputMode m_swapchainOutputMode = SwapchainOutputMode::SdrSrgb;
    bool m_pendingPresent = false;           // set by BeginFrame when Acquire succeeds; cleared by Present (submit uses imageAvailable/renderFinished)
    struct GraphicsPipelineCacheEntry {
        void* vkPipeline = nullptr;
        uint32_t refCount = 0;
    };
    std::unordered_map<uint64_t, GraphicsPipelineCacheEntry> m_graphicsPipelineCache;
    std::unique_ptr<CommandListVK> m_immediateCommandList;
    /** Nested editor slices (SceneView, GameView, MaterialPreview): innermost is active for GetRecordingCommandListVK. */
    std::vector<ICommandList*> m_transientRecordingStack;
    GpuFramePhase m_gpuFramePhase = GpuFramePhase::BetweenFrames;
    /** Slice CBs allocated from frameContexts[i]'s pool; clear only when re-entering slot i after vkWaitForFences(i). */
    std::array<std::vector<std::unique_ptr<ICommandList>>, kMaxFrames> m_frameSliceCommandListsByFrame;
    IBuffer* m_fullscreenQuadVBO = nullptr;
    IBuffer* m_fullscreenQuadIBO = nullptr;
    ITexture* m_fallbackShaderResourceTexture2D = nullptr;
    ITexture* m_fallbackShaderResourceTextureCube = nullptr;
    IRenderTarget* m_sharedShadowMapArrayDummyRt = nullptr;
    bool m_sharedShadowMapArrayDummyBarrierDone = false;
    AllocatorVK m_allocatorVK;
    UploadHeapVK m_uploadHeap;
    DescriptorAllocatorVK m_descriptorAllocator;
    DescriptorCacheVK m_descriptorCache;
    BarrierTrackerVK m_barrierTracker;
    std::function<void(void*)> m_postBlitCallback;
    bool m_frameOpen = false;               // true between BeginFrame() and EndFrame()
    bool m_deviceLost = false;              // set on VK_ERROR_DEVICE_LOST; VkDevice unusable until full re-init
    void* m_imGuiDescriptorPool = nullptr;  // VkDescriptorPool for ImGui
    DeletionQueueVK m_deletionQueue;
    bool m_supportsExtendedDynamicState = false;
    /** True when VkPhysicalDeviceVulkan12Features::drawIndirectCount is enabled (vkCmd*IndirectCount). */
    bool m_supportsDrawIndirectCount = false;
    /** From `vkGetPhysicalDeviceFeatures2` after query (optional stages for pipelines). */
    bool m_supportsTessellationShader = false;
    bool m_supportsGeometryShader = false;
    bool m_supportsOcclusionQuery = false;
    bool m_supportsHostQueryReset = false;
    bool m_supportsPipelineStatisticsQuery = false;
    bool m_supportsSynchronization2 = false;
    bool m_supportsBufferDeviceAddress = false;
    bool m_supportsSwapchainMaintenance1 = false;
    bool m_supportsSwapchainColorspace = false;
    bool m_supportsPresentWait = false;
    bool m_supportsDescriptorBuffer = false;
    bool m_supportsGraphicsPipelineLibrary = false;
    bool m_supportsPipelineExecutableProperties = false;
    bool m_supportsDeviceFault = false;
    uint32_t m_descriptorBufferCombinedImgSize = 0;
    uint32_t m_descriptorBufferStorageBufSize = 0;
    uint64_t m_descriptorBufferOffsetAlignment = 256;
    void* m_pfnReleaseSwapchainImagesEXT = nullptr; // PFN_vkReleaseSwapchainImagesEXT
    void* m_pfnWaitForPresentKHR = nullptr;         // PFN_vkWaitForPresentKHR
    void* m_pfnGetDescriptorSetLayoutSizeEXT = nullptr;          // PFN_vkGetDescriptorSetLayoutSizeEXT
    void* m_pfnGetDescriptorSetLayoutBindingOffsetEXT = nullptr; // PFN_vkGetDescriptorSetLayoutBindingOffsetEXT
    void* m_pfnGetDescriptorEXT = nullptr;                        // PFN_vkGetDescriptorEXT
    void* m_pfnCmdBindDescriptorBuffersEXT = nullptr;            // PFN_vkCmdBindDescriptorBuffersEXT
    void* m_pfnCmdSetDescriptorBufferOffsetsEXT = nullptr;       // PFN_vkCmdSetDescriptorBufferOffsetsEXT
    void* m_pfnGetPipelineExecutablePropertiesKHR = nullptr;     // PFN_vkGetPipelineExecutablePropertiesKHR
    void* m_pfnGetDeviceFaultInfoEXT = nullptr;                  // PFN_vkGetDeviceFaultInfoEXT
    void* m_pfnCmdDrawMeshTasksEXT = nullptr;                    // PFN_vkCmdDrawMeshTasksEXT
    void* m_pfnCmdDrawMeshTasksIndirectEXT = nullptr;            // PFN_vkCmdDrawMeshTasksIndirectEXT
    void* m_pfnCmdSetFragmentShadingRateKHR = nullptr;           // PFN_vkCmdSetFragmentShadingRateKHR
    void* m_pfnCreateAccelerationStructureKHR = nullptr;         // PFN_vkCreateAccelerationStructureKHR
    void* m_pfnDestroyAccelerationStructureKHR = nullptr;        // PFN_vkDestroyAccelerationStructureKHR
    void* m_pfnCmdBuildAccelerationStructuresKHR = nullptr;      // PFN_vkCmdBuildAccelerationStructuresKHR
    void* m_pfnGetAccelerationStructureBuildSizesKHR = nullptr;  // PFN_vkGetAccelerationStructureBuildSizesKHR
    void* m_pfnGetAccelerationStructureDeviceAddressKHR = nullptr; // PFN_vkGetAccelerationStructureDeviceAddressKHR
    bool m_supportsMeshShader = false;
    bool m_supportsMeshTaskShader = false;
    bool m_supportsVariableRateShading = false;
    bool m_supportsRayQuery = false;
    uint64_t m_rayTracingMaxPrimitiveCount = 0;
    uint32_t m_rayTracingScratchAlignment = 0;
    uint32_t m_vrsMinTexelWidth = 1;
    uint32_t m_vrsMinTexelHeight = 1;
    uint32_t m_vrsMaxTexelWidth = 1;
    uint32_t m_vrsMaxTexelHeight = 1;
    uint32_t m_meshShaderMaxOutputVertices = 0;
    uint32_t m_meshShaderMaxOutputPrimitives = 0;
    uint32_t m_meshShaderMaxPayloadAndSharedMemoryBytes = 0;
    bool m_loggedPipelineExecutableSample = false;
    /** One-shot: `vkGetDeviceFaultInfoEXT` after `VK_ERROR_DEVICE_LOST` (best-effort; spec requires lost device). */
    bool m_deviceFaultInfoLogged = false;
    void* m_crossQueueReleasePool = nullptr;   // VkCommandPool (graphics family)
    void* m_crossQueueReleaseCmd = nullptr;    // VkCommandBuffer
    void* m_crossQueueAcquirePool = nullptr;   // VkCommandPool (compute family)
    void* m_crossQueueAcquireCmd = nullptr;    // VkCommandBuffer
    bool m_crossQueueBridgeReady = false;
    /** Descriptor sets to free after 3 frame boundaries (so no CB still references them). */
    std::vector<IDescriptorSet*> m_pendingDescriptorSetsToFree;
    uint32_t m_ticksUntilDescriptorSetFree = 0;
    struct RetiredTextureEntry {
        TextureVK* texture = nullptr;
        uint32_t retireFrameNumber = 0;
    };
    /** Texture wrappers are retired after N frames to avoid CPU-side stale-pointer UAF. */
    std::vector<RetiredTextureEntry> m_pendingTextureWrapperRetires;
    void RetirePendingTextureWrappers(bool forceAll);
    std::unordered_map<uint64_t, uint32_t> m_accelerationStructureBackingRefCount;
    struct RetiredAccelerationStructureEntry {
        AccelerationStructureVK* accelerationStructure = nullptr;
        uint32_t retireFrameNumber = 0;
    };
    std::vector<RetiredAccelerationStructureEntry> m_pendingAccelerationStructureWrapperRetires;
    void RetirePendingAccelerationStructureWrappers(bool forceAll);
    bool CreateCrossQueueBridgeResources();
    void DestroyCrossQueueBridge();
    void EnsureImGuiDescriptorPool();
    /** HDR/SDR swapchain compose: fullscreen shader (lazy); invalidated when swapchain VkFormat changes. */
    void DestroyPresentResolveResources();
    bool EnsurePresentResolveResources();
    IPipeline* m_presentResolvePipeline = nullptr;
    uint32_t m_presentResolveSwapchainVkFormat = 0;
    IBuffer* m_presentResolveFrameUBO = nullptr;
    IDescriptorSet* m_presentResolveFrameSet = nullptr;
    std::array<IDescriptorSet*, kMaxFrames> m_presentResolvePostSets = {};
    IBindlessHeap* m_sharedBindlessHeap = nullptr;
    std::unordered_map<ITexture*, BindlessHandle> m_sharedBindlessTextureHandles;
    std::mutex m_sharedBindlessMutex;
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
