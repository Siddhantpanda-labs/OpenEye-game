#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceGL;
struct PipelineGL;
struct PipelineLayoutGL;
struct DescriptorSetGL;
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
    void ClearDefaultFramebuffer(const ClearValue& clear) override;
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void SetDepthTest(bool enable) override;
    void SetDepthWrite(bool enable) override;
    void SetDepthFuncLess(bool less) override;
    void SetCullFace(bool enable) override;
    void SetCullFrontFace(bool cullFront) override;
    void SetBlend(bool enable) override;

    void BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) override;
    void BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) override;

    void BindPipeline(IPipeline* pipeline) override;
    void BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) override;
    void BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset) override;
    void DrawFullscreenQuad(IPipeline* pipeline) override;
    void BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) override;
    void BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) override;
    void BindAtomicCounterBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) override;
    void BindTexture(ITexture* texture, uint32_t binding) override;
    void UnbindTexture(uint32_t binding) override;
    void BindDescriptorSet(IDescriptorSet* set) override;
    void BindBindlessHeap(IBindlessHeap* heap) override;

    void SetPushConstants(uint32_t offset, uint32_t size, const void* data) override;

    void SetUniformInt(const char* name, int value) override;
    void SetUniformUint(const char* name, uint32_t value) override;
    void SetUniformFloat(const char* name, float value) override;
    void SetUniformVec2(const char* name, const float* data) override;
    void SetUniformVec3(const char* name, const float* data) override;
    void SetUniformVec4(const char* name, const float* data) override;
    void SetUniformMat4(const char* name, const float* data) override;
    void SetUniformMat4Array(const char* name, const float* data, size_t count) override;
    void SetUniformHandle64(const char* name, uint64_t handle) override;
    void SetSubroutineIndices(ShaderStage stage, uint32_t count, const uint32_t* indices) override;

    void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void DrawArraysIndirect(IBuffer* indirectBuffer, size_t offset) override;
    void DrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset) override;
    void MultiDrawArraysIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) override;
    void MultiDrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) override;
    void MultiDrawArraysIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override;
    void MultiDrawIndexedIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override;
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    void DispatchIndirect(IBuffer* indirectBuffer, size_t offset) override;

    void BeginQuery(IQueryPool* pool, uint32_t index) override;
    void EndQuery(IQueryPool* pool, uint32_t index) override;

    void BindTransformFeedbackBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) override;
    void BeginTransformFeedback(PrimitiveTopology primitiveMode) override;
    void EndTransformFeedback() override;

    void BeginConditionalRender(IQueryPool* pool, uint32_t index) override;
    void EndConditionalRender() override;

    IFence* InsertFence() override;

    void CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) override;

private:
    static constexpr uint32_t kMaxTransformFeedbackBuffers = 4;
    struct TFBinding {
        BufferGL* buffer = nullptr;
        size_t offset = 0;
        size_t size = 0;
    };
    TFBinding m_tfBuffers[kMaxTransformFeedbackBuffers];
    bool m_inConditionalRender = false;
    DeviceGL* m_device = nullptr;
    unsigned int m_vao = 0;  // VAO required in core profile for vertex attribs and draw
    IBuffer* m_bonesUBO = nullptr;
    size_t m_bonesUBOSize = 0;
    PipelineGL* m_currentPipeline = nullptr;
    IRenderTarget* m_currentRT = nullptr;
    BufferGL* m_currentVBO = nullptr;
    BufferGL* m_currentIBO = nullptr;
    IndexType m_currentIndexType = IndexType::UInt32;
    bool m_cullFrontFace = false;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
