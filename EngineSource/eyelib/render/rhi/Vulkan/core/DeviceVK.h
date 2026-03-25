#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/Vulkan/commands/CommandListVK.h"
#include "eyelib/render/rhi/Vulkan/core/FrameContextVK.h"
#include "eyelib/render/rhi/Vulkan/core/SwapchainVK.h"
#include "eyelib/render/rhi/Vulkan/memory/DeletionQueueVK.h"
#include "eyelib/render/rhi/Vulkan/core/QueuesVK.h"
#include "eyelib/render/rhi/Vulkan/core/InstanceVK.h"
#include "eyelib/render/rhi/Vulkan/core/PhysicalDeviceVK.h"
#include "eyelib/render/rhi/Vulkan/memory/AllocatorVK.h"
#include "eyelib/render/rhi/Vulkan/memory/UploadHeapVK.h"
#include "eyelib/render/rhi/Vulkan/descriptors/DescriptorAllocatorVK.h"
#include "eyelib/render/rhi/Vulkan/descriptors/DescriptorCacheVK.h"
#include "eyelib/render/rhi/Vulkan/barriers/BarrierTrackerVK.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace render {
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

/** Vulkan RHI device. Handles are opaque (void*) to avoid requiring vulkan.h in this header. */
struct DeviceVK : IDevice {
    DeviceVK();
    ~DeviceVK() override;

    const RHICapabilities& GetCapabilities() const override;
    ::eyelib::render::RenderBackend GetRenderBackend() const override { return ::eyelib::render::RenderBackend::Vulkan; }
    const char* GetShaderBackendName() const override;
    const char* GetShaderVertexExtension() const override;
    const char* GetShaderFragmentExtension() const override;
    bool ResolveShaderPath(const char* logicalName, ShaderStage stage, std::string& outPath) override;

    ICommandList* CreateCommandList() override;
    void DestroyCommandList(ICommandList* list) override;
    ICommandList* GetImmediateCommandList() override;
    void SubmitCommandList(ICommandList* list) override;

    void* GetBarrierTracker() override;
    void TransitionTextureToState(ITexture* texture, ResourceState newState) override;
    void FlushBarrierTracker(ICommandList* cmd) override;

    IBuffer* CreateBuffer(const BufferDesc& desc, const void* initialData) override;
    void DestroyBuffer(IBuffer* buffer) override;

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

    ITexture* CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) override;

    IQueryPool* CreateQueryPool(QueryType type, uint32_t count) override;
    void DestroyQueryPool(IQueryPool* pool) override;

    IPipelineLayout* CreatePipelineLayout(const PipelineLayoutDesc& desc) override;
    void DestroyPipelineLayout(IPipelineLayout* layout) override;
    IDescriptorSet* CreateDescriptorSet(IPipelineLayout* layout) override;
    void DestroyDescriptorSet(IDescriptorSet* set) override;
    IPipelineLayout* GetResourceSetLayout(ResourceSetType type) override;
    IDescriptorSet* GetEmptyResourceSet() override { return m_emptyResourceSet; }

    IBindlessHeap* CreateBindlessHeap() override;
    void DestroyBindlessHeap(IBindlessHeap* heap) override;

    int GetMaxSamples(int requestedSamples) override;

    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;
    void OnWindowResized(uint32_t width, uint32_t height) override;
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
    /** Default pipeline layout: PushOnly (0 sets, push constants only). Used for graphics pipelines without a resource-space layout (e.g. BRDF LUT, fullscreen quads). */
    void* GetDefaultPipelineLayout() const { return m_defaultPipelineLayout; }
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

private:
    void QueryCapabilities();

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
    /** Wrappers for GetResourceSetLayout (single-set IPipelineLayout per type); do not own set layouts. */
    IPipelineLayout* m_resourceSetLayouts[10] = {};  // Frame, PostPass, ..., IBLPass, OutlinePost
    void* m_emptyResourceSetLayout = nullptr;       // VkDescriptorSetLayout (0 bindings, placeholder for compute sets 1–3)
    IPipelineLayout* m_emptyResourceSetLayoutWrapper = nullptr;
    IDescriptorSet* m_emptyResourceSet = nullptr;   // one set from m_emptyResourceSetLayout, for BindDescriptorSets placeholder
    rhi::IBuffer* m_legacyComputeParamsUBO = nullptr; // 256 bytes, binding 1 (ParamsBlock for light_cull)
    void* m_timelineSemaphore = nullptr;     // VkSemaphore (timeline, for fence sync)
    uint64_t m_timelineValue = 0;           // next value to signal on submit
    void* m_surface = nullptr;              // VkSurfaceKHR
    SwapchainVK m_swapchain;                 // swapchain, images, views, framebuffers, render pass
    bool m_pendingPresent = false;           // set by BeginFrame when Acquire succeeds; cleared by Present (submit uses imageAvailable/renderFinished)
    struct GraphicsPipelineCacheEntry {
        void* vkPipeline = nullptr;
        uint32_t refCount = 0;
    };
    std::unordered_map<uint64_t, GraphicsPipelineCacheEntry> m_graphicsPipelineCache;
    std::unique_ptr<CommandListVK> m_immediateCommandList;
    IBuffer* m_fullscreenQuadVBO = nullptr;
    IBuffer* m_fullscreenQuadIBO = nullptr;
    AllocatorVK m_allocatorVK;
    UploadHeapVK m_uploadHeap;
    DescriptorAllocatorVK m_descriptorAllocator;
    DescriptorCacheVK m_descriptorCache;
    BarrierTrackerVK m_barrierTracker;
    std::function<void(void*)> m_postBlitCallback;
    bool m_frameOpen = false;               // true between BeginFrame() and EndFrame()
    void* m_imGuiDescriptorPool = nullptr;  // VkDescriptorPool for ImGui
    DeletionQueueVK m_deletionQueue;
    bool m_supportsExtendedDynamicState = false;
    /** Descriptor sets to free after 3 frame boundaries (so no CB still references them). */
    std::vector<IDescriptorSet*> m_pendingDescriptorSetsToFree;
    uint32_t m_ticksUntilDescriptorSetFree = 0;
    void EnsureImGuiDescriptorPool();
};

} // namespace rhi
} // namespace render
} // namespace eyelib
