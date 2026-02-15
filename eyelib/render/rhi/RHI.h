#pragma once

#include "eyelib/render/rhi/RHITypes.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

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
    virtual IPipeline* CreateGraphicsPipelineFromName(const char* shaderName, const VertexLayout& layout, PrimitiveTopology topology) = 0;
    /** E5.3: Create pipeline from cached binary (format + data from GetPipelineBinary). Returns nullptr if unsupported or invalid. */
    virtual IPipeline* CreateGraphicsPipelineFromBinary(const void* data, size_t size, uint32_t binaryFormat, const VertexLayout& layout, PrimitiveTopology topology) = 0;
    /** E5.3: Retrieve pipeline binary for caching. Returns false if unsupported; outFormat and outData set on success. */
    virtual bool GetPipelineBinary(IPipeline* pipeline, std::vector<uint8_t>& outData, uint32_t& outFormat) = 0;
    virtual IPipeline* CreateComputePipeline(void* computeShader) = 0;
    virtual void DestroyPipeline(IPipeline* pipeline) = 0;

    virtual IRenderTarget* CreateRenderTarget(uint32_t width, uint32_t height,
                                              const std::vector<RHITextureFormat>& colorFormats,
                                              RHITextureFormat depthFormat,
                                              uint32_t samples = 1) = 0;
    /** Create render target from existing texture (face for cubemap, mip level). */
    virtual IRenderTarget* CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) = 0;
    virtual void DestroyRenderTarget(IRenderTarget* rt) = 0;

    virtual IQueryPool* CreateQueryPool(QueryType type, uint32_t count) = 0;
    virtual void DestroyQueryPool(IQueryPool* pool) = 0;

    /** Create pipeline layout (describes bindings for descriptor sets). Used for Vulkan VkPipelineLayout; OpenGL emulates. */
    virtual IPipelineLayout* CreatePipelineLayout(const PipelineLayoutDesc& desc) = 0;
    virtual void DestroyPipelineLayout(IPipelineLayout* layout) = 0;
    /** Create descriptor set from layout; fill with UpdateTexture/UpdateUniformBuffer/UpdateStorageBuffer, then BindDescriptorSet(cmd, set). */
    virtual IDescriptorSet* CreateDescriptorSet(IPipelineLayout* layout) = 0;
    virtual void DestroyDescriptorSet(IDescriptorSet* set) = 0;

    /** Create bindless heap for allocate-once bindless texture/buffer handles. Returns nullptr if !GetCapabilities().supportsBindless. */
    virtual IBindlessHeap* CreateBindlessHeap() = 0;
    virtual void DestroyBindlessHeap(IBindlessHeap* heap) = 0;

    /** Maximum supported MSAA samples; clamp requested samples to this. Returns 0 if no MSAA support. */
    virtual int GetMaxSamples(int requestedSamples) = 0;

    /** Wrap an existing backend texture (e.g. GL id as void*) for use with BindTexture. Caller owns the GL texture; wrapper is not destroyed with glDelete. */
    virtual ITexture* CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) = 0;

    virtual void Present() = 0;
    /** Blit color from given render target to the default framebuffer (e.g. for present). */
    virtual void BlitToDefaultFramebuffer(IRenderTarget* src) = 0;
    /** Blit from backend-native framebuffer (e.g. GL FBO id as void*) to default framebuffer. */
    virtual void BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) = 0;
    /** Blit from src to dst (full size). colorOnly: true = color bit, false = color + depth. */
    virtual void Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) = 0;
    /** Blit from backend-native framebuffer (e.g. GL FBO id as void*) to RHI render target. */
    virtual void BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) = 0;
    /** Blit only depth from src to dst (e.g. GBuffer depth to HDR RT for transparency). Backend may no-op if not supported. */
    virtual void BlitDepthOnly(IRenderTarget* src, IRenderTarget* dst) = 0;
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
    virtual void SetDepthTest(bool enable) = 0;
    virtual void SetDepthWrite(bool enable) = 0;
    virtual void SetDepthFuncLess(bool less) = 0;  // true = GL_LESS, false = GL_LEQUAL etc.
    virtual void SetCullFace(bool enable) = 0;
    /** When cull face is enabled: true = cull front faces (e.g. shadow map), false = cull back. */
    virtual void SetCullFrontFace(bool cullFront) = 0;
    /** Enable/disable alpha blending (src_alpha, one_minus_src_alpha). For transparency pass. */
    virtual void SetBlend(bool enable) = 0;

    virtual void BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) = 0;
    virtual void BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) = 0;

    virtual void BindPipeline(IPipeline* pipeline) = 0;
    virtual void BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset = 0) = 0;
    virtual void BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset = 0) = 0;
    virtual void BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset = 0, uint32_t size = 0) = 0;
    virtual void BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset = 0, uint32_t size = 0) = 0;
    /** Bind buffer for atomic counter access (ARB_shader_atomic_counters). Gated by supportsAtomicCounters. */
    virtual void BindAtomicCounterBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) = 0;
    virtual void BindTexture(ITexture* texture, uint32_t binding) = 0;
    virtual void UnbindTexture(uint32_t binding) = 0;
    /** Bind a descriptor set (all textures/buffers updated on it). Pipeline must be bound first; set layout must match pipeline. */
    virtual void BindDescriptorSet(IDescriptorSet* set) = 0;
    /** Bind bindless heap for the next draw/dispatch (bindless pipelines only). No-op if heap is null or bindless not supported. */
    virtual void BindBindlessHeap(IBindlessHeap* heap) = 0;

    /** Push constants: small per-draw data. See RHIUniformLayout.h for layout. Size must be <= kPushConstantMaxSize. */
    virtual void SetPushConstants(uint32_t offset, uint32_t size, const void* data) = 0;

    virtual void SetUniformInt(const char* name, int value) = 0;
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
    /** Single indirect draw (arrays). Buffer must contain DrawArraysIndirectCommand at offset. Gated by caps. */
    virtual void DrawArraysIndirect(IBuffer* indirectBuffer, size_t offset) = 0;
    /** Single indirect draw (indexed). Index buffer must be bound. Buffer contains DrawIndexedIndirectCommand at offset. */
    virtual void DrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset) = 0;
    /** Multi-draw indirect (arrays). drawCount commands at stride bytes apart. Gated by supportsMultiDrawIndirect. */
    virtual void MultiDrawArraysIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) = 0;
    /** Multi-draw indirect (indexed). Index buffer must be bound. Gated by supportsMultiDrawIndirect. */
    virtual void MultiDrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) = 0;
    virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

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
};

// ---- Descriptor set ----

struct IDescriptorSet {
    virtual ~IDescriptorSet() = default;
    virtual void UpdateTexture(uint32_t binding, ITexture* texture) = 0;
    virtual void UpdateUniformBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset = 0, uint32_t size = 0) = 0;
    virtual void UpdateStorageBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset = 0, uint32_t size = 0) = 0;
};

// ---- Pipeline ----

struct IPipeline {
    virtual ~IPipeline() = default;
    /** False when pipeline was created but has no valid backend program. */
    virtual bool HasValidProgram() const { return true; }
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
