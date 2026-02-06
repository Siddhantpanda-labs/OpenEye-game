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
    virtual void DestroyRenderTarget(IRenderTarget* rt) = 0;

    virtual IQueryPool* CreateQueryPool(QueryType type, uint32_t count) = 0;
    virtual void DestroyQueryPool(IQueryPool* pool) = 0;

    virtual void Present() = 0;
};

// ---- Command list ----

struct ICommandList {
    virtual ~ICommandList() = default;

    virtual void Begin() = 0;
    virtual void End() = 0;

    virtual void SetRenderTarget(IRenderTarget* rt) = 0;
    virtual void SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) = 0;
    virtual void ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) = 0;

    virtual void BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) = 0;
    virtual void BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) = 0;

    virtual void BindPipeline(IPipeline* pipeline) = 0;
    virtual void BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset = 0) = 0;
    virtual void BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset = 0) = 0;
    virtual void BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset = 0, uint32_t size = 0) = 0;
    virtual void BindTexture(ITexture* texture, uint32_t binding) = 0;

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
