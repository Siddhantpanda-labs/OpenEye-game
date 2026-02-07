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
struct IRenderTarget;
struct IQueryPool;

// ---- Device ----

struct IDevice {
    virtual ~IDevice() = default;

    virtual ICommandList* GetImmediateCommandList() = 0;
    virtual void SubmitCommandList(ICommandList* list) = 0;

    virtual IBuffer* CreateBuffer(const BufferDesc& desc, const void* initialData = nullptr) = 0;
    virtual void DestroyBuffer(IBuffer* buffer) = 0;

    virtual ITexture* CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) = 0;
    virtual void DestroyTexture(ITexture* texture) = 0;

    virtual IShader* CreateShader(ShaderStage stage, const char* source, size_t sourceLen) = 0;
    virtual void DestroyShader(IShader* shader) = 0;

    virtual IPipeline* CreateGraphicsPipeline(const PipelineDesc& desc) = 0;
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
    /** Blit only depth from native FBO to dst (for deferred: copy GBuffer depth to HDR for transparency). */
    virtual void BlitDepthOnlyFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst) = 0;
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
    virtual void BindTexture(ITexture* texture, uint32_t binding) = 0;
    virtual void UnbindTexture(uint32_t binding) = 0;

    virtual void SetUniformInt(const char* name, int value) = 0;
    virtual void SetUniformFloat(const char* name, float value) = 0;
    virtual void SetUniformVec2(const char* name, const float* data) = 0;
    virtual void SetUniformVec3(const char* name, const float* data) = 0;
    virtual void SetUniformVec4(const char* name, const float* data) = 0;
    virtual void SetUniformMat4(const char* name, const float* data) = 0;
    virtual void SetUniformMat4Array(const char* name, const float* data, size_t count) = 0;

    virtual void Draw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0) = 0;
    virtual void DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) = 0;
    virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

    virtual void BeginQuery(IQueryPool* pool, uint32_t index) = 0;
    virtual void EndQuery(IQueryPool* pool, uint32_t index) = 0;

    virtual void CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) = 0;
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

// ---- Pipeline ----

struct IPipeline {
    virtual ~IPipeline() = default;
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
