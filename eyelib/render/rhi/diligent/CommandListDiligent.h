#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceDiligent;

/** Stub command list: all operations no-op. */
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
    void BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) override;
    void BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) override;
    void BindTexture(ITexture* texture, uint32_t binding) override;
    void UnbindTexture(uint32_t binding) override;

    void SetPushConstants(uint32_t offset, uint32_t size, const void* data) override;

    void SetUniformInt(const char* name, int value) override;
    void SetUniformFloat(const char* name, float value) override;
    void SetUniformVec2(const char* name, const float* data) override;
    void SetUniformVec3(const char* name, const float* data) override;
    void SetUniformVec4(const char* name, const float* data) override;
    void SetUniformMat4(const char* name, const float* data) override;
    void SetUniformMat4Array(const char* name, const float* data, size_t count) override;

    void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

    void BeginQuery(IQueryPool* pool, uint32_t index) override;
    void EndQuery(IQueryPool* pool, uint32_t index) override;

    void CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) override;

private:
    void EnsureConstantBuffers();
    void CommitUniformsAndResources();

    DeviceDiligent* m_device = nullptr;
    IPipeline* m_currentPipeline = nullptr;
    IBuffer* m_currentVertexBuffer = nullptr;
    uint32_t m_currentVertexOffset = 0;
    uint32_t m_currentVertexBinding = 0;
    IBuffer* m_currentIndexBuffer = nullptr;
    IndexType m_currentIndexType = IndexType::UInt32;
    uint32_t m_currentIndexOffset = 0;

    static constexpr size_t kConstantsSize = 256u;
    static constexpr size_t kMaterialSize = 256u;
    static constexpr size_t kBonesSize = 8192u;
    std::vector<uint8_t> m_constantsStaging;
    std::vector<uint8_t> m_materialStaging;
    std::vector<uint8_t> m_bonesStaging;
    void* m_pConstantsBuffer = nullptr;
    void* m_pMaterialBuffer = nullptr;
    void* m_pBonesBuffer = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
