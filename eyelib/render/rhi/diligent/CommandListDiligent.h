#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceDiligent;
struct PipelineDiligent;
struct BufferDiligent;

/** Command list for Diligent: records draw and state; implements constants via SRB + dynamic CBs. */
struct CommandListDiligent : ICommandList {
    explicit CommandListDiligent(DeviceDiligent* device);
    ~CommandListDiligent() override;

    void Begin() override;
    void End() override;

    void SetRenderTarget(IRenderTarget* rt) override;
    void SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) override;
    void ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) override;
    void ClearDefaultFramebuffer(const ClearValue& clear) override;
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void SetViewportIndexed(uint32_t index, uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void SetDepthTest(bool enable) override;
    void SetDepthWrite(bool enable) override;
    void SetDepthFuncLess(bool less) override;
    void SetCullFace(bool enable) override;
    void SetCullFrontFace(bool cullFront) override;
    void SetBlend(bool enable) override;
    void SetPolygonOffsetLine(bool enable, float factor, float units) override;

    void BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) override;
    void BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) override;
    void MemoryBarrierStorage() override;
    void BindPixelPackBufferAsSSBO(void* pboHandle, uint32_t slot, size_t size) override;

    void BindPipeline(IPipeline* pipeline) override;
    void BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) override;
    void DrawFullscreenQuad(IPipeline* pipeline) override;
    void BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset) override;
    void BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) override;
    void BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) override;
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

    void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void DrawIndexedInstanced(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t instanceCount) override;
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

    void BindAtomicCounterBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) override;
    void BindTransformFeedbackBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) override;
    void BeginTransformFeedback(PrimitiveTopology primitiveMode) override;
    void EndTransformFeedback() override;
    void BeginConditionalRender(IQueryPool* pool, uint32_t index) override;
    void EndConditionalRender() override;
    void SetSubroutineIndices(ShaderStage stage, uint32_t count, const uint32_t* indices) override;
    IFence* InsertFence() override;

    void CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) override;

private:
    void EnsureDynamicConstantBuffers();
    void CommitShaderResources();

    DeviceDiligent* m_device = nullptr;
    PipelineDiligent* m_currentPipeline = nullptr;
    IBuffer* m_currentVertexBuffer = nullptr;
    uint32_t m_currentVertexOffset = 0;
    IBuffer* m_currentIndexBuffer = nullptr;
    IndexType m_currentIndexType = IndexType::UInt16;
    uint32_t m_currentIndexOffset = 0;

    static constexpr size_t kConstantsSize = 288u;   // 4 mat4 + extra for skinned
    static constexpr size_t kMaterialSize = 384u;
    static constexpr size_t kBonesSize = 128u * 64u; // 128 * float4x4
    alignas(16) uint8_t m_constantsStaging[kConstantsSize]{};
    alignas(16) uint8_t m_materialStaging[kMaterialSize]{};
    alignas(16) uint8_t m_bonesStaging[kBonesSize]{};
    bool m_constantsDirty = false;
    bool m_materialDirty = false;
    bool m_bonesDirty = false;

    void* m_cbConstants = nullptr;  // Diligent::IBuffer* (owned)
    void* m_cbMaterial = nullptr;
    void* m_cbBones = nullptr;

    static constexpr uint32_t kMaxTextureSlots = 8u;
    ITexture* m_boundTextures[kMaxTextureSlots] = {};
};

} // namespace rhi
} // namespace render
} // namespace eyelib
