#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <vulkan/vulkan_core.h>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVk;
struct PipelineVk;
struct RenderTargetVk;
struct BufferVk;
struct TextureVk;

struct CommandListVk : ICommandList {
    explicit CommandListVk(DeviceVk* device);
    ~CommandListVk() override;

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

    /** Submit the current command buffer to the given queue. */
    void Submit(VkQueue queue);
    /** Copy from buffer to image (for initial texture upload). */
    void CopyBufferToImage(IBuffer* src, size_t srcOffset, ITexture* dst, uint32_t mip, uint32_t layer, uint32_t face);
    /** Present swapchain (acquire, blit if needed, present). */
    void Present(VkSwapchainKHR swapchain, VkQueue presentQueue, uint32_t imageCount);
    /** Blit current render target to swapchain. */
    void BlitToSwapchain(DeviceVk* device, uint32_t srcWidth, uint32_t srcHeight);
    void Blit(RenderTargetVk* src, RenderTargetVk* dst, bool colorOnly);
    void BlitFromNative(void* nativeFbo, RenderTargetVk* dst, bool colorOnly);
    void BlitDepthOnlyFromNative(void* nativeFbo, RenderTargetVk* dst);

private:
    void FlushDescriptorSet();
    void EnsureCommandBuffer();

    DeviceVk* m_device = nullptr;
    VkCommandBuffer m_cmdBuf = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
    bool m_recording = false;
    PipelineVk* m_currentPipeline = nullptr;
    RenderTargetVk* m_currentRT = nullptr;
    BufferVk* m_currentVBO = nullptr;
    BufferVk* m_currentIBO = nullptr;
    IndexType m_currentIndexType = IndexType::UInt32;
    bool m_cullFrontFace = false;
    bool m_depthTest = true;
    bool m_depthWrite = true;
    bool m_depthFuncLess = true;
    bool m_blend = false;
    VkViewport m_viewport = {};
    VkRect2D m_scissor = {};
};

} // namespace rhi
} // namespace render
} // namespace eyelib
