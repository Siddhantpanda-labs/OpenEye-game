#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceBGFX;
struct PipelineBGFX;
struct BufferBGFX;
struct TextureBGFX;
struct RenderTargetBGFX;

/** BGFX backend: records into current frame encoder (Begin/End scope). */
struct CommandListBGFX : ICommandList {
    explicit CommandListBGFX(DeviceBGFX* device);
    ~CommandListBGFX() override;

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
    void SetPackedUniforms();
    static constexpr int kMaxTextureBindings = 8;
    DeviceBGFX* m_device = nullptr;
    const char* m_samplerNameForUnit[kMaxTextureBindings] = {};  // Set by SetInt(name, unit)
    void* m_encoder = nullptr;  // bgfx::Encoder*
    uint16_t m_currentProgramIdx = 0xffff;  // bgfx ProgramHandle.idx
    uint16_t m_currentVbIdx = 0xffff;       // bgfx VertexBufferHandle.idx
    uint16_t m_currentIbIdx = 0xffff;       // bgfx IndexBufferHandle.idx
    uint16_t m_viewId = 0;
    uint32_t m_viewport[4] = {0, 0, 0, 0};
    uint64_t m_stateOverride = 0;
    bool m_depthTest = true;
    bool m_depthWrite = true;
    bool m_depthFuncLess = true;
    bool m_cullFace = false;
    bool m_cullFront = false;
    bool m_blend = false;
    ITexture* m_boundTextures[kMaxTextureBindings] = {};
    float m_tonemapParams[4] = {1.f, 1.f, 0.f, 0.f};   // x=exposure, y=useACES
    float m_bloomExtractParams[4] = {1.f, 0.f, 0.f, 0.f}; // x=threshold
    float m_blurParams[4] = {0.f, 0.f, 0.f, 1.f};      // xy=resolution, z=horizontal, w=intensity
    float m_bloomCompositeParams[4] = {0.5f, 0.f, 0.f, 0.f}; // x=bloomIntensity
};

} // namespace rhi
} // namespace render
} // namespace eyelib
