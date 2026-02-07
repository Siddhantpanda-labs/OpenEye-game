#include "eyelib/render/rhi/Vulkan/CommandListVk.h"
#include "eyelib/render/rhi/Vulkan/DeviceVk.h"
#include "eyelib/render/rhi/Vulkan/PipelineVk.h"
#include "eyelib/render/rhi/Vulkan/RenderTargetVk.h"
#include "eyelib/render/rhi/Vulkan/BufferVk.h"
#include "eyelib/render/rhi/Vulkan/TextureVk.h"
#include "eyelib/render/rhi/Vulkan/QueryPoolVk.h"
#include "eyelib/render/rhi/Vulkan/VkEnums.h"
#include <vulkan/vulkan_core.h>
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

namespace {

constexpr size_t kPushConstantSize = 256;

} // namespace

CommandListVk::CommandListVk(DeviceVk* device) : m_device(device) {
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device->GetDevice(), &fenceInfo, nullptr, &m_fence);
    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(device->GetDevice(), &semInfo, nullptr, &m_semaphore);
}

CommandListVk::~CommandListVk() {
    if (m_device && m_device->GetDevice()) {
        if (m_fence != VK_NULL_HANDLE) { vkDestroyFence(m_device->GetDevice(), m_fence, nullptr); m_fence = VK_NULL_HANDLE; }
        if (m_semaphore != VK_NULL_HANDLE) { vkDestroySemaphore(m_device->GetDevice(), m_semaphore, nullptr); m_semaphore = VK_NULL_HANDLE; }
    }
}

void CommandListVk::EnsureCommandBuffer() {
    if (m_cmdBuf != VK_NULL_HANDLE) return;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_device->GetGraphicsCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_device->GetDevice(), &allocInfo, &m_cmdBuf);
}

void CommandListVk::Begin() {
    m_currentPipeline = nullptr;
    m_currentRT = nullptr;
    m_currentVBO = nullptr;
    m_currentIBO = nullptr;
    m_recording = true;
    EnsureCommandBuffer();
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_cmdBuf, &beginInfo);
}

void CommandListVk::End() {
    if (!m_recording || m_cmdBuf == VK_NULL_HANDLE) return;
    vkEndCommandBuffer(m_cmdBuf);
    m_recording = false;
}

void CommandListVk::Submit(VkQueue queue) {
    if (m_cmdBuf == VK_NULL_HANDLE) return;
    vkWaitForFences(m_device->GetDevice(), 1, &m_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device->GetDevice(), 1, &m_fence);
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_cmdBuf;
    vkQueueSubmit(queue, 1, &submit, m_fence);
    m_cmdBuf = VK_NULL_HANDLE;
}

void CommandListVk::SetRenderTarget(IRenderTarget* rt) {
    SetRenderTarget(rt, 0);
}

void CommandListVk::SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) {
    (void)colorAttachmentIndex;
    auto* r = static_cast<RenderTargetVk*>(rt);
    m_currentRT = r;
    if (!r || m_cmdBuf == VK_NULL_HANDLE) return;
    VkRenderPassBeginInfo rpBegin = {};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = r->m_renderPass;
    rpBegin.framebuffer = r->m_framebuffer;
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = {r->m_width, r->m_height};
    rpBegin.clearValueCount = 0;
    vkCmdBeginRenderPass(m_cmdBuf, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandListVk::ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) {
    if (!rt || m_cmdBuf == VK_NULL_HANDLE) return;
    auto* r = static_cast<RenderTargetVk*>(rt);
    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{clear.color[0], clear.color[1], clear.color[2], clear.color[3]}};
    clearValues[1].depthStencil = {clear.depth, clear.stencil};
    VkRenderPassBeginInfo rpBegin = {};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = r->m_renderPass;
    rpBegin.framebuffer = r->m_framebuffer;
    rpBegin.renderArea.extent = {r->m_width, r->m_height};
    rpBegin.clearValueCount = (r->m_depthTexture || r->m_externalDepthTexture) ? 2 : 1;
    rpBegin.pClearValues = clearValues;
    vkCmdBeginRenderPass(m_cmdBuf, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(m_cmdBuf);
    m_currentRT = nullptr;
}

void CommandListVk::ClearDefaultFramebuffer(const ClearValue& clear) {
    (void)clear;
}

void CommandListVk::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    m_viewport = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height), 0.f, 1.f};
    m_scissor = {{static_cast<int32_t>(x), static_cast<int32_t>(y)}, {width, height}};
    if (m_cmdBuf != VK_NULL_HANDLE) {
        vkCmdSetViewport(m_cmdBuf, 0, 1, &m_viewport);
        vkCmdSetScissor(m_cmdBuf, 0, 1, &m_scissor);
    }
}

void CommandListVk::SetDepthTest(bool enable) { m_depthTest = enable; }
void CommandListVk::SetDepthWrite(bool enable) { m_depthWrite = enable; }
void CommandListVk::SetDepthFuncLess(bool less) { m_depthFuncLess = less; }
void CommandListVk::SetCullFace(bool enable) { (void)enable; }
void CommandListVk::SetCullFrontFace(bool cullFront) { m_cullFrontFace = cullFront; }
void CommandListVk::SetBlend(bool enable) { m_blend = enable; }

void CommandListVk::BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) {
    (void)buffer;
    (void)fromState;
    (void)toState;
}

void CommandListVk::BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) {
    if (!texture || m_cmdBuf == VK_NULL_HANDLE) return;
    auto* t = static_cast<TextureVk*>(texture);
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = vk::VkEnums::ImageLayout(fromState);
    barrier.newLayout = vk::VkEnums::ImageLayout(toState);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = t->m_image;
    barrier.subresourceRange.aspectMask = (t->m_format == RHITextureFormat::Depth24 || t->m_format == RHITextureFormat::Depth24Stencil8) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    if (t->m_format == RHITextureFormat::Depth24Stencil8) barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = vk::VkEnums::AccessFlags(fromState);
    barrier.dstAccessMask = vk::VkEnums::AccessFlags(toState);
    vkCmdPipelineBarrier(m_cmdBuf, vk::VkEnums::PipelineStage(fromState), vk::VkEnums::PipelineStage(toState), 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void CommandListVk::FlushDescriptorSet() {}

void CommandListVk::BindPipeline(IPipeline* pipeline) {
    m_currentPipeline = static_cast<PipelineVk*>(pipeline);
    if (!m_currentPipeline || m_cmdBuf == VK_NULL_HANDLE) return;
    vkCmdBindPipeline(m_cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_currentPipeline->m_pipeline);
}

void CommandListVk::BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) {
    m_currentVBO = static_cast<BufferVk*>(buffer);
    if (!m_currentVBO || m_cmdBuf == VK_NULL_HANDLE) return;
    VkDeviceSize off = offset;
    vkCmdBindVertexBuffers(m_cmdBuf, binding, 1, &m_currentVBO->m_buffer, &off);
}

void CommandListVk::BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset) {
    m_currentIBO = static_cast<BufferVk*>(buffer);
    m_currentIndexType = indexType;
    if (!m_currentIBO || m_cmdBuf == VK_NULL_HANDLE) return;
    vkCmdBindIndexBuffer(m_cmdBuf, m_currentIBO->m_buffer, offset, indexType == IndexType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}

void CommandListVk::BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) {
    if (!buffer || m_cmdBuf == VK_NULL_HANDLE || !m_currentPipeline) return;
    auto* b = static_cast<BufferVk*>(buffer);
    VkDescriptorSet set = VK_NULL_HANDLE;
    (void)set;
    (void)b;
    (void)binding;
    (void)offset;
    (void)size;
    FlushDescriptorSet();
}

void CommandListVk::BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) {
    (void)buffer;
    (void)binding;
    (void)offset;
    (void)size;
}

void CommandListVk::BindTexture(ITexture* texture, uint32_t binding) {
    (void)texture;
    (void)binding;
    FlushDescriptorSet();
}

void CommandListVk::UnbindTexture(uint32_t binding) {
    (void)binding;
}

void CommandListVk::SetUniformInt(const char* name, int value) { (void)name; (void)value; }
void CommandListVk::SetUniformFloat(const char* name, float value) { (void)name; (void)value; }
void CommandListVk::SetUniformVec2(const char* name, const float* data) { (void)name; (void)data; }
void CommandListVk::SetUniformVec3(const char* name, const float* data) { (void)name; (void)data; }
void CommandListVk::SetUniformVec4(const char* name, const float* data) { (void)name; (void)data; }
void CommandListVk::SetUniformMat4(const char* name, const float* data) {
    if (!m_currentPipeline || !data || m_cmdBuf == VK_NULL_HANDLE) return;
    vkCmdPushConstants(m_cmdBuf, m_currentPipeline->m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 64, data);
}
void CommandListVk::SetUniformMat4Array(const char* name, const float* data, size_t count) {
    (void)name;
    if (!m_currentPipeline || !data || count == 0 || m_cmdBuf == VK_NULL_HANDLE) return;
    if (count <= 4)
        vkCmdPushConstants(m_cmdBuf, m_currentPipeline->m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<uint32_t>(count * 64), data);
}

void CommandListVk::Draw(uint32_t vertexCount, uint32_t firstVertex) {
    if (m_cmdBuf == VK_NULL_HANDLE) return;
    vkCmdDraw(m_cmdBuf, vertexCount, 1, firstVertex, 0);
}

void CommandListVk::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    if (m_cmdBuf == VK_NULL_HANDLE) return;
    vkCmdDrawIndexed(m_cmdBuf, indexCount, 1, firstIndex, vertexOffset, 0);
}

void CommandListVk::DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    DrawIndexed(indexCount, firstIndex, vertexOffset);
}

void CommandListVk::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    if (m_cmdBuf == VK_NULL_HANDLE) return;
    vkCmdDispatch(m_cmdBuf, groupCountX, groupCountY, groupCountZ);
}

void CommandListVk::BeginQuery(IQueryPool* pool, uint32_t index) {
    if (!pool || m_cmdBuf == VK_NULL_HANDLE) return;
    QueryPoolVk* p = static_cast<QueryPoolVk*>(pool);
    if (index >= p->m_count) return;
    vkCmdBeginQuery(m_cmdBuf, p->m_pool, index, 0);
}

void CommandListVk::EndQuery(IQueryPool* pool, uint32_t index) {
    if (!pool || m_cmdBuf == VK_NULL_HANDLE) return;
    QueryPoolVk* p = static_cast<QueryPoolVk*>(pool);
    if (index >= p->m_count) return;
    vkCmdEndQuery(m_cmdBuf, p->m_pool, index);
}

void CommandListVk::CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) {
    if (!src || !dst || m_cmdBuf == VK_NULL_HANDLE) return;
    auto* s = static_cast<BufferVk*>(src);
    auto* d = static_cast<BufferVk*>(dst);
    if (size == 0) size = s->GetSize();
    VkBufferCopy region = {};
    region.srcOffset = srcOffset;
    region.dstOffset = dstOffset;
    region.size = size;
    vkCmdCopyBuffer(m_cmdBuf, s->m_buffer, d->m_buffer, 1, &region);
}

void CommandListVk::CopyBufferToImage(IBuffer* src, size_t srcOffset, ITexture* dst, uint32_t mip, uint32_t layer, uint32_t face) {
    if (!src || !dst || m_cmdBuf == VK_NULL_HANDLE) return;
    auto* b = static_cast<BufferVk*>(src);
    auto* t = static_cast<TextureVk*>(dst);
    VkBufferImageCopy region = {};
    region.bufferOffset = srcOffset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = mip;
    region.imageSubresource.baseArrayLayer = (t->m_type == TextureType::TextureCube) ? face : layer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {t->m_width, t->m_height, 1};
    vkCmdCopyBufferToImage(m_cmdBuf, b->m_buffer, t->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void CommandListVk::Present(VkSwapchainKHR swapchain, VkQueue presentQueue, uint32_t imageCount) {
    (void)swapchain;
    (void)presentQueue;
    (void)imageCount;
}

void CommandListVk::BlitToSwapchain(DeviceVk* device, uint32_t srcWidth, uint32_t srcHeight) {
    (void)device;
    (void)srcWidth;
    (void)srcHeight;
}

void CommandListVk::Blit(RenderTargetVk* src, RenderTargetVk* dst, bool colorOnly) {
    (void)src;
    (void)dst;
    (void)colorOnly;
}

void CommandListVk::BlitFromNative(void* nativeFbo, RenderTargetVk* dst, bool colorOnly) {
    (void)nativeFbo;
    (void)dst;
    (void)colorOnly;
}

void CommandListVk::BlitDepthOnlyFromNative(void* nativeFbo, RenderTargetVk* dst) {
    (void)nativeFbo;
    (void)dst;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
