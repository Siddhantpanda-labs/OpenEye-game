#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceGL;
struct PipelineGL;
struct RenderTargetGL;
struct BufferGL;
struct TextureGL;
struct QueryPoolGL;

/** OpenGL backend: immediate command list (calls execute as they are recorded). */
struct CommandListGL : ICommandList {
    explicit CommandListGL(DeviceGL* device);
    ~CommandListGL() override;

    void Begin() override;
    void End() override;

    void SetRenderTarget(IRenderTarget* rt) override;
    void SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) override;
    void ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) override;

    void BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) override;
    void BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) override;

    void BindPipeline(IPipeline* pipeline) override;
    void BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) override;
    void BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset) override;
    void BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) override;
    void BindTexture(ITexture* texture, uint32_t binding) override;

    void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

    void BeginQuery(IQueryPool* pool, uint32_t index) override;
    void EndQuery(IQueryPool* pool, uint32_t index) override;

    void CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) override;

private:
    DeviceGL* m_device = nullptr;
    PipelineGL* m_currentPipeline = nullptr;
    IRenderTarget* m_currentRT = nullptr;
    BufferGL* m_currentVBO = nullptr;
    BufferGL* m_currentIBO = nullptr;
    IndexType m_currentIndexType = IndexType::UInt32;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
