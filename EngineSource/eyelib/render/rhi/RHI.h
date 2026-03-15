#pragma once

#include "eyelib/render/rhi/RHITypes.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace render {
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

// ---- Device ----

struct IDevice {
    virtual ~IDevice() = default;

    /** Backend capabilities; use these instead of branching on backend type. */
    virtual const RHICapabilities& GetCapabilities() const = 0;

    /** Shader path resolution: base path is assets/shaders/{backendName}/{shaderLogicalPath}. Extensions are backend-specific (e.g. OpenGL: .vert/.frag; Diligent: .vsh/.psh). */
    virtual const char* GetShaderBackendName() const = 0;
    virtual const char* GetShaderVertexExtension() const = 0;
    virtual const char* GetShaderFragmentExtension() const = 0;
    /** Resolve logical shader path (e.g. passes/gpu_cull/cull_indirect_with_drawlist) to backend-specific asset path. Returns true and sets outPath on success. Used for compute and other single-file shaders. */
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
    /** Replay a deferred list (or no-op for immediate). Must be called on the render thread. Submit lists in order for deterministic execution. */
    virtual void SubmitCommandList(ICommandList* list) = 0;

    virtual IBuffer* CreateBuffer(const BufferDesc& desc, const void* initialData = nullptr) = 0;
    virtual void DestroyBuffer(IBuffer* buffer) = 0;

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
    /** Create pipeline from logical shader name. Backend resolves paths/format (e.g. OpenGL: passes/{name}.vert/.frag). */
    /** When depthStencilOverride is non-null, use it for the pipeline's depth/stencil state. layoutKind: Legacy = legacy set0; other = resource-space layout (Post, Forward, etc.). */
    virtual IPipeline* CreateGraphicsPipelineFromName(const char* shaderName, const VertexLayout& layout, PrimitiveTopology topology, const DepthStencilState* depthStencilOverride = nullptr, PipelineLayoutKind layoutKind = PipelineLayoutKind::Legacy) = 0;
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

    virtual IQueryPool* CreateQueryPool(QueryType type, uint32_t count) = 0;
    virtual void DestroyQueryPool(IQueryPool* pool) = 0;

    /** Create pipeline layout (describes bindings for descriptor sets). Used for Vulkan VkPipelineLayout; OpenGL emulates. */
    virtual IPipelineLayout* CreatePipelineLayout(const PipelineLayoutDesc& desc) = 0;
    virtual void DestroyPipelineLayout(IPipelineLayout* layout) = 0;
    /** Create descriptor set from layout; fill with UpdateTexture/UpdateUniformBuffer/UpdateStorageBuffer, then BindDescriptorSet(cmd, set). */
    virtual IDescriptorSet* CreateDescriptorSet(IPipelineLayout* layout) = 0;
    virtual void DestroyDescriptorSet(IDescriptorSet* set) = 0;
    /** Resource-space set type for binding table (set 0 Frame, set 1 Pass, etc.). Get layout then CreateDescriptorSet(layout). Returns nullptr if unsupported. */
    enum class ResourceSetType : uint32_t { Frame, PostPass, ForwardPass, ShadowPass, LightCullPass, Material, SceneBuffers, DeferredLightingPass, IBLPass };
    virtual IPipelineLayout* GetResourceSetLayout(ResourceSetType type) { (void)type; return nullptr; }

    /** Create bindless heap for allocate-once bindless texture/buffer handles. Returns nullptr if !GetCapabilities().supportsBindless. */
    virtual IBindlessHeap* CreateBindlessHeap() = 0;
    virtual void DestroyBindlessHeap(IBindlessHeap* heap) = 0;

    /** Maximum supported MSAA samples; clamp requested samples to this. Returns 0 if no MSAA support. */
    virtual int GetMaxSamples(int requestedSamples) = 0;

    /** Wrap an existing backend texture (e.g. GL id as void*) for use with BindTexture. Caller owns the GL texture; wrapper is not destroyed with glDelete. */
    virtual ITexture* CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) = 0;

    /** Start frame: device owns command buffer lifetime. Vulkan: reset and begin immediate command buffer. Call once per frame before any recording. Other backends: no-op. */
    virtual void BeginFrame() {}
    /** End frame: end recording and submit. Vulkan: end immediate command buffer and submit. Call after all recording (scene, blit, UI) and before Present(). Other backends: no-op. */
    virtual void EndFrame() {}
    virtual void Present() = 0;
    /** Notify backend that output window framebuffer size changed; backend may recreate swapchain/default backbuffer resources. */
    virtual void OnWindowResized(uint32_t width, uint32_t height) { (void)width; (void)height; }
    /** Wait until all previously submitted GPU work has completed (e.g. glFinish). Use before swap to avoid flicker from presenting before blit completes. */
    virtual void Finish() {}
    /** Optional: barrier tracker for batched transitions (e.g. Vulkan BarrierTrackerVK). Returns nullptr if not supported. */
    virtual void* GetBarrierTracker() { return nullptr; }
    /** Optional: transition texture to state via barrier tracker. No-op if GetBarrierTracker() is null. Used by FrameGraph for batched barriers. */
    virtual void TransitionTextureToState(ITexture* texture, ResourceState newState) { (void)texture; (void)newState; }
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

    virtual void SetRenderTarget(IRenderTarget* rt) = 0;
    virtual void SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) = 0;
    virtual void ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) = 0;
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
    /** Bind bindless heap for the next draw/dispatch (bindless pipelines only). No-op if heap is null or bindless not supported. */
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

// ---- Texture ----

struct ITexture {
    virtual ~ITexture() = default;
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual TextureType GetType() const = 0;
    virtual RHITextureFormat GetFormat() const = 0;
    /** Backend-native texture handle (e.g. GL texture id as void*). For interop only. */
    virtual void* GetNativeTexture() = 0;
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
    virtual uint64_t GetResult(uint32_t index) = 0;  // timestamp in nanoseconds
};

} // namespace rhi
} // namespace render
} // namespace eyelib
