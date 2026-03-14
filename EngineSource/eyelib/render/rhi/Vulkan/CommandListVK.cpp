#include "eyelib/render/rhi/Vulkan/CommandListVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/FenceVK.h"
#include "eyelib/render/rhi/Vulkan/BufferVK.h"
#include "eyelib/render/rhi/Vulkan/TextureVK.h"
#include "eyelib/render/rhi/Vulkan/RenderTargetVK.h"
#include "eyelib/render/rhi/Vulkan/BindlessHeapVK.h"
#include "eyelib/render/rhi/Vulkan/BufferVK.h"
#include "eyelib/render/rhi/Vulkan/DescriptorSetVK.h"
#include "eyelib/render/rhi/Vulkan/PipelineVK.h"
#include "eyelib/render/rhi/Vulkan/VulkanEnums.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

CommandListVK::CommandListVK(DeviceVK* device) : m_device(device) {}

CommandListVK::CommandListVK(DeviceVK* device, void* commandBuffer)
    : m_device(device), m_commandBuffer(commandBuffer), m_isImmediate(false) {}

CommandListVK::~CommandListVK() {
    if (!m_isImmediate && m_commandBuffer && m_device)
        m_device->FreeCommandBuffer(m_commandBuffer);
    m_commandBuffer = nullptr;
}

void CommandListVK::SetCommandBuffer(void* commandBuffer, bool isImmediate) {
    m_commandBuffer = commandBuffer;
    m_isImmediate = isImmediate;
}

void CommandListVK::Begin() {
    if (m_isImmediate && m_device && m_device->IsFrameOpen())
        return;
    m_isRecording = false;
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(m_commandBuffer);
    if (buf == VK_NULL_HANDLE) return;
    vkResetCommandBuffer(buf, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(buf, &beginInfo) == VK_SUCCESS)
        m_isRecording = true;
}

void CommandListVK::End() {
    if (m_isImmediate && m_device && m_device->IsFrameOpen())
        return;
    EndRenderingIfActive();
    if (!m_isRecording) return;
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(m_commandBuffer);
    if (buf != VK_NULL_HANDLE)
        vkEndCommandBuffer(buf);
    m_isRecording = false;
}

void CommandListVK::EndRenderingIfActive() {
    if (!m_currentRenderTarget || !m_commandBuffer || !m_isRecording) return;
    vkCmdEndRendering(static_cast<VkCommandBuffer>(m_commandBuffer));
    m_currentRenderTarget = nullptr;
}

void CommandListVK::SetRenderTarget(IRenderTarget* rt) {
    EndRenderingIfActive();
    if (!rt || !m_isRecording || !m_commandBuffer) return;
    RenderTargetVK* rtVK = static_cast<RenderTargetVK*>(rt);
    const uint32_t colorCount = rtVK->GetColorAttachmentCount();
    if (colorCount == 0) return;
    std::vector<VkRenderingAttachmentInfo> colorAttachments(colorCount);
    for (uint32_t i = 0; i < colorCount; ++i) {
        VkImageView view = static_cast<VkImageView>(rtVK->GetColorImageView(i));
        colorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachments[i].imageView = view;
        colorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachments[i].clearValue.color = { 0.f, 0.f, 0.f, 1.f };
    }
    VkImageView depthView = static_cast<VkImageView>(rtVK->GetDepthImageView());
    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { { 0, 0 }, { rtVK->GetWidth(), rtVK->GetHeight() } };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorCount;
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;
    if (depthView != VK_NULL_HANDLE) {
        VkRenderingAttachmentInfo depthAttachment = {};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = depthView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = { 1.f, 0 };
        renderingInfo.pDepthAttachment = &depthAttachment;
    }
    vkCmdBeginRendering(static_cast<VkCommandBuffer>(m_commandBuffer), &renderingInfo);
    m_currentRenderTarget = rtVK;
}

void CommandListVK::SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) {
    (void)colorAttachmentIndex;
    SetRenderTarget(rt);
}

void CommandListVK::ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) {
    if (!rt || !m_isRecording || !m_commandBuffer) return;
    RenderTargetVK* rtVK = static_cast<RenderTargetVK*>(rt);
    if (rtVK != m_currentRenderTarget) return;
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(m_commandBuffer);
    const uint32_t colorCount = rtVK->GetColorAttachmentCount();
    std::vector<VkClearAttachment> clears;
    for (uint32_t i = 0; i < colorCount; ++i) {
        VkClearAttachment att = {};
        att.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        att.colorAttachment = i;
        att.clearValue.color.float32[0] = clear.color[0];
        att.clearValue.color.float32[1] = clear.color[1];
        att.clearValue.color.float32[2] = clear.color[2];
        att.clearValue.color.float32[3] = clear.color[3];
        clears.push_back(att);
    }
    if (rtVK->GetDepthImageView() != VK_NULL_HANDLE) {
        VkClearAttachment att = {};
        att.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        att.clearValue.depthStencil.depth = clear.depth;
        att.clearValue.depthStencil.stencil = clear.stencil;
        clears.push_back(att);
    }
    if (clears.empty()) return;
    VkClearRect rect = {};
    rect.rect = { { 0, 0 }, { rtVK->GetWidth(), rtVK->GetHeight() } };
    rect.baseArrayLayer = 0;
    rect.layerCount = 1;
    vkCmdClearAttachments(buf, static_cast<uint32_t>(clears.size()), clears.data(), 1, &rect);
}

void CommandListVK::ClearDefaultFramebuffer(const ClearValue& clear) { (void)clear; }

void CommandListVK::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!m_isRecording || !m_commandBuffer) return;
    VkViewport vp = {};
    vp.x = static_cast<float>(x);
    vp.y = static_cast<float>(y);
    vp.width = static_cast<float>(width);
    vp.height = static_cast<float>(height);
    vp.minDepth = 0.f;
    vp.maxDepth = 1.f;
    vkCmdSetViewport(static_cast<VkCommandBuffer>(m_commandBuffer), 0, 1, &vp);
    VkRect2D scissor = { { static_cast<int32_t>(x), static_cast<int32_t>(y) }, { width, height } };
    vkCmdSetScissor(static_cast<VkCommandBuffer>(m_commandBuffer), 0, 1, &scissor);
}

void CommandListVK::SetViewportIndexed(uint32_t index, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!m_isRecording || !m_commandBuffer) return;
    VkViewport vp = {};
    vp.x = static_cast<float>(x);
    vp.y = static_cast<float>(y);
    vp.width = static_cast<float>(width);
    vp.height = static_cast<float>(height);
    vp.minDepth = 0.f;
    vp.maxDepth = 1.f;
    vkCmdSetViewport(static_cast<VkCommandBuffer>(m_commandBuffer), index, 1, &vp);
    VkRect2D scissor = { { static_cast<int32_t>(x), static_cast<int32_t>(y) }, { width, height } };
    vkCmdSetScissor(static_cast<VkCommandBuffer>(m_commandBuffer), index, 1, &scissor);
}
void CommandListVK::SetDepthTest(bool enable) { (void)enable; }
void CommandListVK::SetDepthWrite(bool enable) { (void)enable; }
void CommandListVK::SetDepthFuncLess(bool less) { (void)less; }
void CommandListVK::SetCullFace(bool enable) { (void)enable; }
void CommandListVK::SetCullFrontFace(bool cullFront) { (void)cullFront; }
void CommandListVK::SetBlend(bool enable) { (void)enable; }
void CommandListVK::SetBlendFunc(BlendFactor src, BlendFactor dst) { (void)src; (void)dst; }
void CommandListVK::SetPolygonOffsetLine(bool enable, float factor, float units) { (void)enable; (void)factor; (void)units; }

void CommandListVK::BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) {
    if (!buffer || !m_isRecording || !m_commandBuffer) return;
    BufferVK* bufVK = static_cast<BufferVK*>(buffer);
    VkBuffer vkBuf = static_cast<VkBuffer>(bufVK->m_buffer);
    if (vkBuf == VK_NULL_HANDLE) return;
    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = ToVkAccessFlags(fromState);
    barrier.dstAccessMask = ToVkAccessFlags(toState);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = vkBuf;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
    VkPipelineStageFlags srcStage = ToVkPipelineStage(fromState);
    VkPipelineStageFlags dstStage = ToVkPipelineStage(toState);
    vkCmdPipelineBarrier(static_cast<VkCommandBuffer>(m_commandBuffer), srcStage, dstStage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void CommandListVK::BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) {
    if (!texture || !m_isRecording || !m_commandBuffer) return;
    TextureVK* texVK = static_cast<TextureVK*>(texture);
    VkImage image = static_cast<VkImage>(texVK->m_image);
    if (image == VK_NULL_HANDLE) return;
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = ToVkAccessFlags(fromState);
    barrier.dstAccessMask = ToVkAccessFlags(toState);
    barrier.oldLayout = ToVkImageLayout(fromState);
    barrier.newLayout = ToVkImageLayout(toState);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = ToVkImageAspect(texVK->GetFormat());
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    VkPipelineStageFlags srcStage = ToVkPipelineStage(fromState);
    VkPipelineStageFlags dstStage = ToVkPipelineStage(toState);
    vkCmdPipelineBarrier(static_cast<VkCommandBuffer>(m_commandBuffer), srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void CommandListVK::MemoryBarrierStorage() {
    if (!m_isRecording || !m_commandBuffer) return;
    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    vkCmdPipelineBarrier(static_cast<VkCommandBuffer>(m_commandBuffer), srcStage, dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}
void CommandListVK::BindPixelPackBufferAsSSBO(void* pboHandle, uint32_t slot, size_t size) { (void)pboHandle; (void)slot; (void)size; }

void CommandListVK::BindPipeline(IPipeline* pipeline) {
    if (!pipeline || !m_isRecording || !m_commandBuffer || !m_device) return;
    m_currentPipeline = pipeline;
    PipelineVK* pVK = static_cast<PipelineVK*>(pipeline);
    VkPipeline vkPipe = static_cast<VkPipeline>(pVK->m_pipeline);
    if (vkPipe == VK_NULL_HANDLE) return;
    VkPipelineBindPoint bindPoint = pVK->m_isCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindPipeline(static_cast<VkCommandBuffer>(m_commandBuffer), bindPoint, vkPipe);
    if (!pVK->m_isCompute) {
        VkPipelineLayout defaultLayout = static_cast<VkPipelineLayout>(m_device->GetDefaultPipelineLayout());
        if (defaultLayout != VK_NULL_HANDLE) {
            m_device->UpdateLegacySet0(this);
            VkDescriptorSet set0 = static_cast<VkDescriptorSet>(m_device->GetLegacySet0());
            VkDescriptorSet emptyBindless = static_cast<VkDescriptorSet>(m_device->GetEmptyBindlessSet());
            if (set0 != VK_NULL_HANDLE && emptyBindless != VK_NULL_HANDLE) {
                VkDescriptorSet sets[2] = { set0, emptyBindless };
                vkCmdBindDescriptorSets(static_cast<VkCommandBuffer>(m_commandBuffer), bindPoint, defaultLayout, 0, 2, sets, 0, nullptr);
            }
        }
    }
}
void CommandListVK::BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) {
    if (!m_isRecording || !m_commandBuffer) return;
    if (!buffer) { m_currentVertexBuffer = nullptr; return; }
    BufferVK* buf = static_cast<BufferVK*>(buffer);
    VkBuffer vkBuf = static_cast<VkBuffer>(buf->m_buffer);
    if (vkBuf == VK_NULL_HANDLE) return;
    m_currentVertexBuffer = vkBuf;
    m_currentVertexOffset = offset;
    VkDeviceSize off = offset;
    vkCmdBindVertexBuffers(static_cast<VkCommandBuffer>(m_commandBuffer), binding, 1, &vkBuf, &off);
}
void CommandListVK::BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset) {
    if (!m_isRecording || !m_commandBuffer) return;
    if (!buffer) { m_currentIndexBuffer = nullptr; return; }
    BufferVK* buf = static_cast<BufferVK*>(buffer);
    VkBuffer vkBuf = static_cast<VkBuffer>(buf->m_buffer);
    if (vkBuf == VK_NULL_HANDLE) return;
    m_currentIndexBuffer = vkBuf;
    m_currentIndexOffset = offset;
    m_currentIndexType = indexType;
    vkCmdBindIndexBuffer(static_cast<VkCommandBuffer>(m_commandBuffer), vkBuf, offset, ToVkIndexType(indexType));
}
void CommandListVK::DrawFullscreenQuad(IPipeline* pipeline) {
    if (!pipeline || !m_device || !m_isRecording || !m_commandBuffer) return;
    m_device->EnsureFullscreenQuad();
    IBuffer* vbo = m_device->GetFullscreenQuadVBO();
    IBuffer* ibo = m_device->GetFullscreenQuadIBO();
    if (!vbo || !ibo) return;
    BindVertexBuffer(vbo, 0, 0);
    BindIndexBuffer(ibo, IndexType::UInt16, 0);
    VkPipelineLayout layout = static_cast<VkPipelineLayout>(m_device->GetDefaultPipelineLayout());
    if (layout != VK_NULL_HANDLE) {
        m_device->UpdateLegacySet0(this);
        VkDescriptorSet set0 = static_cast<VkDescriptorSet>(m_device->GetLegacySet0());
        VkDescriptorSet emptyBindless = static_cast<VkDescriptorSet>(m_device->GetEmptyBindlessSet());
        if (set0 != VK_NULL_HANDLE && emptyBindless != VK_NULL_HANDLE) {
            VkDescriptorSet sets[2] = { set0, emptyBindless };
            vkCmdBindDescriptorSets(static_cast<VkCommandBuffer>(m_commandBuffer), VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, sets, 0, nullptr);
        }
        vkCmdPushConstants(static_cast<VkCommandBuffer>(m_commandBuffer), layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128, m_pushConstants);
    }
    vkCmdDrawIndexed(static_cast<VkCommandBuffer>(m_commandBuffer), 6, 1, 0, 0, 0);
}
void CommandListVK::BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) {
    if (binding >= kMaxLegacyBindings) return;
    m_boundUniformBuffers[binding] = buffer;
    m_boundUniformOffsets[binding] = offset;
    m_boundUniformSizes[binding] = size;
}
void CommandListVK::BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) {
    if (binding >= kMaxLegacyBindings) return;
    m_boundStorageBuffers[binding] = buffer;
    m_boundStorageOffsets[binding] = offset;
    m_boundStorageSizes[binding] = size;
}
void CommandListVK::BindAtomicCounterBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) { (void)slot; (void)buffer; (void)offset; (void)size; }
void CommandListVK::BindTexture(ITexture* texture, uint32_t binding) {
    if (binding >= kMaxLegacyBindings) return;
    m_boundTextures[binding] = texture;
}
void CommandListVK::UnbindTexture(uint32_t binding) {
    if (binding < kMaxLegacyBindings) m_boundTextures[binding] = nullptr;
}
ITexture* CommandListVK::GetBoundTexture(uint32_t binding) const {
    return (binding < kMaxLegacyBindings) ? m_boundTextures[binding] : nullptr;
}
void CommandListVK::GetBoundStorageBuffer(uint32_t binding, IBuffer** outBuffer, uint32_t* outOffset, uint32_t* outSize) const {
    if (binding >= kMaxLegacyBindings) { if (outBuffer) *outBuffer = nullptr; return; }
    if (outBuffer) *outBuffer = m_boundStorageBuffers[binding];
    if (outOffset) *outOffset = m_boundStorageOffsets[binding];
    if (outSize) *outSize = m_boundStorageSizes[binding];
}
void CommandListVK::GetBoundUniformBuffer(uint32_t binding, IBuffer** outBuffer, uint32_t* outOffset, uint32_t* outSize) const {
    if (binding >= kMaxLegacyBindings) { if (outBuffer) *outBuffer = nullptr; return; }
    if (outBuffer) *outBuffer = m_boundUniformBuffers[binding];
    if (outOffset) *outOffset = m_boundUniformOffsets[binding];
    if (outSize) *outSize = m_boundUniformSizes[binding];
}
void CommandListVK::BindDescriptorSet(IDescriptorSet* set) {
    if (!set || !m_isRecording || !m_commandBuffer) return;
    DescriptorSetVK* dsVK = static_cast<DescriptorSetVK*>(set);
    VkDescriptorSet vkSet = static_cast<VkDescriptorSet>(dsVK->m_descriptorSet);
    PipelineLayoutVK* layout = dsVK->m_layout;
    if (vkSet == VK_NULL_HANDLE || !layout || layout->m_pipelineLayout == VK_NULL_HANDLE) return;
    VkPipelineLayout pipelineLayout = static_cast<VkPipelineLayout>(layout->m_pipelineLayout);
    VkCommandBuffer cmdBuf = static_cast<VkCommandBuffer>(m_commandBuffer);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &vkSet, 0, nullptr);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &vkSet, 0, nullptr);
}
void CommandListVK::BindBindlessHeap(IBindlessHeap* heap) {
    if (!m_isRecording || !m_commandBuffer || !m_device) return;
    VkPipelineLayout defaultLayout = static_cast<VkPipelineLayout>(m_device->GetDefaultPipelineLayout());
    if (defaultLayout == VK_NULL_HANDLE) return;
    VkDescriptorSet set1 = static_cast<VkDescriptorSet>(m_device->GetEmptyBindlessSet());
    if (heap) {
        BindlessHeapVK* heapVK = static_cast<BindlessHeapVK*>(heap);
        if (heapVK->m_descriptorSet != nullptr)
            set1 = static_cast<VkDescriptorSet>(heapVK->m_descriptorSet);
    }
    vkCmdBindDescriptorSets(static_cast<VkCommandBuffer>(m_commandBuffer), VK_PIPELINE_BIND_POINT_GRAPHICS, defaultLayout, 1, 1, &set1, 0, nullptr);
    vkCmdBindDescriptorSets(static_cast<VkCommandBuffer>(m_commandBuffer), VK_PIPELINE_BIND_POINT_COMPUTE, defaultLayout, 1, 1, &set1, 0, nullptr);
}

void CommandListVK::SetPushConstants(uint32_t offset, uint32_t size, const void* data) {
    if (offset + size <= sizeof(m_pushConstants) && data)
        std::memcpy(m_pushConstants + offset, data, size);
}

// ParamsBlock (light_cull) offsets in m_computeParamsData
static constexpr uint32_t kOffsetView = 0;
static constexpr uint32_t kOffsetProjection = 64;
static constexpr uint32_t kOffsetTileCountX = 128;
static constexpr uint32_t kOffsetTileCountY = 132;
static constexpr uint32_t kOffsetSliceCount = 136;
static constexpr uint32_t kOffsetLogNear = 140;
static constexpr uint32_t kOffsetLogFar = 144;
static constexpr uint32_t kOffsetNearPlane = 148;
static constexpr uint32_t kOffsetFarPlane = 152;
static constexpr uint32_t kOffsetScreenWidth = 156;
static constexpr uint32_t kOffsetScreenHeight = 160;
static constexpr uint32_t kOffsetPointLightCount = 164;
static constexpr uint32_t kOffsetSpotLightCount = 168;

void CommandListVK::SetUniformInt(const char* name, int value) {
    if (!name) return;
    if (std::strcmp(name, "u_UseIBL") == 0)
        *reinterpret_cast<int*>(m_pushConstants + 0) = value;
    else if (std::strcmp(name, "u_UseOcclusionCull") == 0)
        *reinterpret_cast<int*>(m_pushConstants + 4) = value;
    else if (std::strcmp(name, "u_PointLightCount") == 0)
        *reinterpret_cast<int*>(m_computeParamsData + kOffsetPointLightCount) = value;
    else if (std::strcmp(name, "u_SpotLightCount") == 0)
        *reinterpret_cast<int*>(m_computeParamsData + kOffsetSpotLightCount) = value;
}
void CommandListVK::SetUniformUint(const char* name, uint32_t value) {
    if (!name) return;
    if (std::strcmp(name, "u_NumDraws") == 0)
        *reinterpret_cast<uint32_t*>(m_pushConstants + 0) = value;
    else if (std::strcmp(name, "u_TileCountX") == 0)
        *reinterpret_cast<uint32_t*>(m_computeParamsData + kOffsetTileCountX) = value;
    else if (std::strcmp(name, "u_TileCountY") == 0)
        *reinterpret_cast<uint32_t*>(m_computeParamsData + kOffsetTileCountY) = value;
    else if (std::strcmp(name, "u_SliceCount") == 0)
        *reinterpret_cast<uint32_t*>(m_computeParamsData + kOffsetSliceCount) = value;
}
void CommandListVK::SetUniformFloat(const char* name, float value) {
    if (!name) return;
    if (std::strcmp(name, "u_LogNear") == 0)
        *reinterpret_cast<float*>(m_computeParamsData + kOffsetLogNear) = value;
    else if (std::strcmp(name, "u_LogFar") == 0)
        *reinterpret_cast<float*>(m_computeParamsData + kOffsetLogFar) = value;
    else if (std::strcmp(name, "u_NearPlane") == 0)
        *reinterpret_cast<float*>(m_computeParamsData + kOffsetNearPlane) = value;
    else if (std::strcmp(name, "u_FarPlane") == 0)
        *reinterpret_cast<float*>(m_computeParamsData + kOffsetFarPlane) = value;
    else if (std::strcmp(name, "u_ScreenWidth") == 0)
        *reinterpret_cast<float*>(m_computeParamsData + kOffsetScreenWidth) = value;
    else if (std::strcmp(name, "u_ScreenHeight") == 0)
        *reinterpret_cast<float*>(m_computeParamsData + kOffsetScreenHeight) = value;
}
void CommandListVK::SetUniformVec2(const char* name, const float* data) {
    if (!name || !data) return;
    if (std::strcmp(name, "u_DepthSize") == 0)
        std::memcpy(m_pushConstants + 8, data, 8);
}
void CommandListVK::SetUniformVec3(const char* name, const float* data) { (void)name; (void)data; }
void CommandListVK::SetUniformVec4(const char* name, const float* data) { (void)name; (void)data; }
void CommandListVK::SetUniformMat4(const char* name, const float* data) {
    if (!name || !data) return;
    const size_t mat4Bytes = 16 * sizeof(float);
    if (std::strcmp(name, "u_ViewProj") == 0)
        std::memcpy(m_pushConstants + 16, data, mat4Bytes);
    else if (std::strcmp(name, "u_InverseViewMatrix") == 0)
        std::memcpy(m_pushConstants + 0, data, mat4Bytes);
    else if (std::strcmp(name, "u_InverseProjectionMatrix") == 0)
        std::memcpy(m_pushConstants + 64, data, mat4Bytes);
    else if (std::strcmp(name, "u_View") == 0)
        std::memcpy(m_computeParamsData + kOffsetView, data, mat4Bytes);
    else if (std::strcmp(name, "u_Projection") == 0)
        std::memcpy(m_computeParamsData + kOffsetProjection, data, mat4Bytes);
}
void CommandListVK::SetUniformMat4Array(const char* name, const float* data, size_t count) { (void)name; (void)data; (void)count; }
void CommandListVK::SetUniformHandle64(const char* name, uint64_t handle) { (void)name; (void)handle; }
void CommandListVK::SetSubroutineIndices(ShaderStage stage, uint32_t count, const uint32_t* indices) { (void)stage; (void)count; (void)indices; }

void CommandListVK::Draw(uint32_t vertexCount, uint32_t firstVertex) {
    if (!m_isRecording || !m_commandBuffer) return;
    vkCmdDraw(static_cast<VkCommandBuffer>(m_commandBuffer), vertexCount, 1, firstVertex, 0);
}
void CommandListVK::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    if (!m_isRecording || !m_commandBuffer) return;
    vkCmdDrawIndexed(static_cast<VkCommandBuffer>(m_commandBuffer), indexCount, 1, firstIndex, vertexOffset, 0);
}
void CommandListVK::DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    DrawIndexed(indexCount, firstIndex, vertexOffset);
}
void CommandListVK::DrawIndexedInstanced(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t instanceCount) {
    if (!m_isRecording || !m_commandBuffer) return;
    vkCmdDrawIndexed(static_cast<VkCommandBuffer>(m_commandBuffer), indexCount, instanceCount, firstIndex, vertexOffset, 0);
}
void CommandListVK::DrawArraysIndirect(IBuffer* indirectBuffer, size_t offset) { (void)indirectBuffer; (void)offset; }
void CommandListVK::DrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset) { (void)indirectBuffer; (void)offset; }
void CommandListVK::MultiDrawArraysIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) { (void)indirectBuffer; (void)offset; (void)drawCount; (void)stride; }
void CommandListVK::MultiDrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) { (void)indirectBuffer; (void)offset; (void)drawCount; (void)stride; }
void CommandListVK::MultiDrawArraysIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) { (void)indirectBuffer; (void)indirectOffset; (void)countBuffer; (void)countBufferOffset; (void)maxDrawCount; (void)stride; }
void CommandListVK::MultiDrawIndexedIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) { (void)indirectBuffer; (void)indirectOffset; (void)countBuffer; (void)countBufferOffset; (void)maxDrawCount; (void)stride; }
void CommandListVK::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    if (!m_isRecording || !m_commandBuffer || !m_device) return;
    PipelineVK* pVK = m_currentPipeline ? static_cast<PipelineVK*>(m_currentPipeline) : nullptr;
    if (pVK && pVK->m_isCompute && pVK->m_pipeline != VK_NULL_HANDLE) {
        VkPipelineLayout computeLayout = static_cast<VkPipelineLayout>(m_device->GetComputePipelineLayout());
        if (computeLayout != VK_NULL_HANDLE) {
            m_device->UpdateComputeSet0(this);
            VkDescriptorSet computeSet0 = static_cast<VkDescriptorSet>(m_device->GetComputeSet0());
            VkDescriptorSet emptyBindless = static_cast<VkDescriptorSet>(m_device->GetEmptyBindlessSet());
            VkDescriptorSet sets[2] = { computeSet0, emptyBindless };
            vkCmdBindDescriptorSets(static_cast<VkCommandBuffer>(m_commandBuffer), VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 2, sets, 0, nullptr);
            vkCmdPushConstants(static_cast<VkCommandBuffer>(m_commandBuffer), computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 16, m_pushConstants);
        }
        uint32_t x = (groupCountX > 0) ? groupCountX : 1u;
        uint32_t y = (groupCountY > 0) ? groupCountY : 1u;
        uint32_t z = (groupCountZ > 0) ? groupCountZ : 1u;
        vkCmdDispatch(static_cast<VkCommandBuffer>(m_commandBuffer), x, y, z);
    }
}
void CommandListVK::DispatchIndirect(IBuffer* indirectBuffer, size_t offset) { (void)indirectBuffer; (void)offset; }

void CommandListVK::BeginQuery(IQueryPool* pool, uint32_t index) { (void)pool; (void)index; }
void CommandListVK::EndQuery(IQueryPool* pool, uint32_t index) { (void)pool; (void)index; }

void CommandListVK::BindTransformFeedbackBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) { (void)slot; (void)buffer; (void)offset; (void)size; }
void CommandListVK::BeginTransformFeedback(PrimitiveTopology primitiveMode) { (void)primitiveMode; }
void CommandListVK::EndTransformFeedback() {}

void CommandListVK::BeginConditionalRender(IQueryPool* pool, uint32_t index) { (void)pool; (void)index; }
void CommandListVK::EndConditionalRender() {}

IFence* CommandListVK::InsertFence() {
    if (!m_device || !m_device->GetTimelineSemaphore()) return nullptr;
    if (m_pendingTimelineValue == 0)
        m_pendingTimelineValue = m_device->ReserveTimelineValue();
    return new FenceVK(m_device, m_pendingTimelineValue);
}

void CommandListVK::CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) { (void)src; (void)srcOffset; (void)dst; (void)dstOffset; (void)size; }

} // namespace rhi
} // namespace render
} // namespace eyelib
