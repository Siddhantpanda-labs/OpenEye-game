#include "eyelib/render/rhi/Vulkan/commands/CommandListVK.h"
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/sync/FenceVK.h"
#include "eyelib/render/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/render/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/render/rhi/Vulkan/resources/RenderTargetVK.h"
#include "eyelib/render/rhi/Vulkan/descriptors/BindlessHeapVK.h"
#include "eyelib/render/rhi/Vulkan/descriptors/DescriptorSetVK.h"
#include "eyelib/render/rhi/Vulkan/pipelines/PipelineVK.h"
#include "eyelib/render/rhi/Vulkan/utils/VulkanEnums.h"
#include <vulkan/vulkan.h>
#include "eyelib/core/Log.h"
#include <cstdint>
#include <vector>
#include <cstring>
#include <cassert>

namespace eyelib {
namespace render {
namespace rhi {

namespace {
const char* PipelineLayoutKindName(PipelineLayoutKind kind) {
    switch (kind) {
    case PipelineLayoutKind::PushOnly: return "PushOnly";
    case PipelineLayoutKind::Forward: return "Forward";
    case PipelineLayoutKind::Post: return "Post";
    case PipelineLayoutKind::Shadow: return "Shadow";
    case PipelineLayoutKind::GBuffer: return "GBuffer";
    case PipelineLayoutKind::DeferredLighting: return "DeferredLighting";
    case PipelineLayoutKind::ComputeLightCull: return "ComputeLightCull";
    case PipelineLayoutKind::ComputeGpuCull: return "ComputeGpuCull";
    case PipelineLayoutKind::IBL: return "IBL";
    case PipelineLayoutKind::OutlinePost: return "OutlinePost";
    default: return "Unknown";
    }
}

/** Bindless texture heap set index in layouts from VulkanResourceSpaces CreateForwardPipelineLayout / CreateGBufferPipelineLayout. */
static uint32_t BindlessHeapDescriptorSetIndex(PipelineLayoutKind kind) {
    switch (kind) {
    case PipelineLayoutKind::Forward:
        return 3u;
    case PipelineLayoutKind::GBuffer:
        return 2u;
    default:
        return UINT32_MAX;
    }
}
} // namespace

CommandListVK::CommandListVK(DeviceVK* device) : m_device(device) {}

CommandListVK::CommandListVK(DeviceVK* device, void* commandBuffer, void* commandPool)
    : m_device(device), m_commandBuffer(commandBuffer), m_commandPool(commandPool), m_isImmediate(false) {}

CommandListVK::~CommandListVK() {
    if (!m_isImmediate && m_commandBuffer && m_commandPool && m_device)
        m_device->FreeCommandBuffer(m_commandBuffer, m_commandPool);
    m_commandBuffer = nullptr;
    m_commandPool = nullptr;
}

void CommandListVK::SetCommandBuffer(void* commandBuffer, bool isImmediate) {
    m_commandBuffer = commandBuffer;
    m_isImmediate = isImmediate;
}

void CommandListVK::ResetCpuRecordingStateAfterPoolReset() {
    m_isRenderingActive = false;
    m_currentRenderTarget = nullptr;
    m_currentVertexBuffer = nullptr;
    m_currentVertexOffset = 0;
    m_currentIndexBuffer = nullptr;
    m_currentIndexOffset = 0;
    m_currentIndexType = IndexType::UInt16;
    m_currentPipeline = nullptr;
    std::memset(m_pushConstants, 0, sizeof(m_pushConstants));
    for (uint32_t i = 0; i < kMaxLegacyBindings; ++i) {
        m_boundTextures[i] = nullptr;
        m_boundStorageBuffers[i] = nullptr;
        m_boundStorageOffsets[i] = 0;
        m_boundStorageSizes[i] = 0;
        m_boundUniformBuffers[i] = nullptr;
        m_boundUniformOffsets[i] = 0;
        m_boundUniformSizes[i] = 0;
    }
    std::memset(m_computeParamsData, 0, sizeof(m_computeParamsData));
    m_pendingTimelineValue = 0;
    m_depthTestEnabled = true;
    m_depthWriteEnabled = true;
    m_depthFuncLess = true;
    m_cullFaceEnabled = false;
    m_cullFrontFace = false;
    m_pushModelSet = false;
}

void CommandListVK::Begin() {
    if (m_isImmediate && m_device && m_device->IsFrameOpen())
        return;
    m_isRecording = false;
    // Stateless rule: clear all bound state so no state survives across frames.
    ResetCpuRecordingStateAfterPoolReset();
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
    if (!m_isRenderingActive || !m_commandBuffer || !m_isRecording) return;
    vkCmdEndRendering(static_cast<VkCommandBuffer>(m_commandBuffer));
    m_isRenderingActive = false;
    m_currentRenderTarget = nullptr;
}

void CommandListVK::BeginRenderingWithTarget(RenderTargetVK* rtVK, bool doTransitions) {
    if (!rtVK || !m_isRecording || !m_commandBuffer || !m_device) return;
    const uint32_t colorCount = rtVK->GetColorAttachmentCount();
    VkImageView depthView = static_cast<VkImageView>(rtVK->GetDepthImageView());
    if (colorCount == 0 && depthView == VK_NULL_HANDLE) return;
    const uint32_t rtWidth = rtVK->GetWidth();
    const uint32_t rtHeight = rtVK->GetHeight();
    if (rtWidth == 0 || rtHeight == 0) return;

    if (doTransitions) {
        for (uint32_t i = 0; i < colorCount; ++i) {
            ITexture* tex = rtVK->GetColorTexture(i);
            if (tex) m_device->TransitionTextureToState(tex, ResourceState::RenderTarget);
        }
        ITexture* depthTex = rtVK->GetDepthTexture();
        if (depthTex) m_device->TransitionTextureToState(depthTex, ResourceState::DepthWrite);
        m_device->FlushBarrierTracker(this);
    }

    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    if (colorCount > 0) {
        colorAttachments.resize(colorCount);
    }
    for (uint32_t i = 0; i < colorCount; ++i) {
        VkImageView view = static_cast<VkImageView>(rtVK->GetColorImageView(i));
        if (view == VK_NULL_HANDLE) return;
        colorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachments[i].imageView = view;
        colorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachments[i].clearValue.color = { 0.f, 0.f, 0.f, 1.f };
    }

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { { 0, 0 }, { rtWidth, rtHeight } };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorCount;
    renderingInfo.pColorAttachments = colorAttachments.empty() ? nullptr : colorAttachments.data();
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;
    VkRenderingAttachmentInfo depthAttachment = {};
    if (depthView != VK_NULL_HANDLE) {
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = depthView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = { 1.f, 0 };
        renderingInfo.pDepthAttachment = &depthAttachment;
    }
    vkCmdBeginRendering(static_cast<VkCommandBuffer>(m_commandBuffer), &renderingInfo);
    m_isRenderingActive = true;
    m_currentRenderTarget = rtVK;
}

void CommandListVK::SetRenderTarget(IRenderTarget* rt) {
    EndRenderingIfActive();
    if (!rt || !m_isRecording || !m_commandBuffer) return;
    RenderTargetVK* rtVK = static_cast<RenderTargetVK*>(rt);
    BeginRenderingWithTarget(rtVK, true);
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
        ITexture* ctex = rtVK->GetColorTexture(i);
        const RHITextureFormat fmt = ctex ? ctex->GetFormat() : RHITextureFormat::RGBA8;
        if (fmt == RHITextureFormat::R32UI) {
            att.clearValue.color.uint32[0] = clear.colorUint;
            att.clearValue.color.uint32[1] = 0;
            att.clearValue.color.uint32[2] = 0;
            att.clearValue.color.uint32[3] = 0;
        } else {
            att.clearValue.color.float32[0] = clear.color[0];
            att.clearValue.color.float32[1] = clear.color[1];
            att.clearValue.color.float32[2] = clear.color[2];
            att.clearValue.color.float32[3] = clear.color[3];
        }
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

void CommandListVK::ClearRenderTargetColorOnly(IRenderTarget* rt, const ClearValue& clear) {
    if (!rt || !m_isRecording || !m_commandBuffer) return;
    RenderTargetVK* rtVK = static_cast<RenderTargetVK*>(rt);
    if (rtVK != m_currentRenderTarget) return;
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(m_commandBuffer);
    const uint32_t colorCount = rtVK->GetColorAttachmentCount();
    std::vector<VkClearAttachment> clears;
    clears.reserve(colorCount);
    for (uint32_t i = 0; i < colorCount; ++i) {
        VkClearAttachment att = {};
        att.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        att.colorAttachment = i;
        ITexture* ctex = rtVK->GetColorTexture(i);
        const RHITextureFormat fmt = ctex ? ctex->GetFormat() : RHITextureFormat::RGBA8;
        if (fmt == RHITextureFormat::R32UI) {
            att.clearValue.color.uint32[0] = clear.colorUint;
            att.clearValue.color.uint32[1] = 0;
            att.clearValue.color.uint32[2] = 0;
            att.clearValue.color.uint32[3] = 0;
        } else {
            att.clearValue.color.float32[0] = clear.color[0];
            att.clearValue.color.float32[1] = clear.color[1];
            att.clearValue.color.float32[2] = clear.color[2];
            att.clearValue.color.float32[3] = clear.color[3];
        }
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
void CommandListVK::SetDepthTest(bool enable) {
    m_depthTestEnabled = enable;
    if (!m_isRecording || !m_commandBuffer || !m_device || !m_device->SupportsExtendedDynamicState()) return;
    vkCmdSetDepthTestEnable(static_cast<VkCommandBuffer>(m_commandBuffer), enable ? VK_TRUE : VK_FALSE);
}

void CommandListVK::SetDepthWrite(bool enable) {
    m_depthWriteEnabled = enable;
    if (!m_isRecording || !m_commandBuffer || !m_device || !m_device->SupportsExtendedDynamicState()) return;
    vkCmdSetDepthWriteEnable(static_cast<VkCommandBuffer>(m_commandBuffer), enable ? VK_TRUE : VK_FALSE);
}

void CommandListVK::SetDepthFuncLess(bool less) {
    m_depthFuncLess = less;
    if (!m_isRecording || !m_commandBuffer || !m_device || !m_device->SupportsExtendedDynamicState()) return;
    const VkCompareOp compareOp = less ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_LESS_OR_EQUAL;
    vkCmdSetDepthCompareOp(static_cast<VkCommandBuffer>(m_commandBuffer), compareOp);
}

void CommandListVK::SetCullFace(bool enable) {
    m_cullFaceEnabled = enable;
    if (!m_isRecording || !m_commandBuffer || !m_device || !m_device->SupportsExtendedDynamicState()) return;
    VkCullModeFlags mode = VK_CULL_MODE_NONE;
    if (enable)
        mode = m_cullFrontFace ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
    vkCmdSetCullMode(static_cast<VkCommandBuffer>(m_commandBuffer), mode);
}

void CommandListVK::SetCullFrontFace(bool cullFront) {
    m_cullFrontFace = cullFront;
    if (!m_isRecording || !m_commandBuffer || !m_device || !m_device->SupportsExtendedDynamicState()) return;
    VkCullModeFlags mode = VK_CULL_MODE_NONE;
    if (m_cullFaceEnabled)
        mode = cullFront ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
    vkCmdSetCullMode(static_cast<VkCommandBuffer>(m_commandBuffer), mode);
}
void CommandListVK::SetBlend(bool enable) { (void)enable; }
void CommandListVK::SetBlendFunc(BlendFactor src, BlendFactor dst) { (void)src; (void)dst; }
void CommandListVK::SetPolygonOffsetLine(bool enable, float factor, float units) { (void)enable; (void)factor; (void)units; }

void CommandListVK::BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) {
    if (!buffer || !m_isRecording || !m_commandBuffer) return;
    if (m_isRenderingActive) {
        static int warnCount = 0;
        if (warnCount++ < 8)
            core::Log::Warn("Vulkan BarrierBuffer called during active rendering; ending render scope explicitly. Move barriers outside render passes.");
    }
    EndRenderingIfActive();
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
    (void)fromState;
    if (m_isRenderingActive) {
        static int warnCount = 0;
        if (warnCount++ < 8)
            core::Log::Warn("Vulkan BarrierTexture called during active rendering; ending render scope explicitly. Move barriers outside render passes.");
    }
    EndRenderingIfActive();
    if (!m_device) return;
    m_device->TransitionTextureToState(texture, toState);
    m_device->FlushBarrierTracker(this);
}

void CommandListVK::MemoryBarrierStorage() {
    if (!m_isRecording || !m_commandBuffer) return;
    if (m_isRenderingActive) {
        static int warnCount = 0;
        if (warnCount++ < 8)
            core::Log::Warn("Vulkan MemoryBarrierStorage called during active rendering; ending render scope explicitly. Move barriers outside render passes.");
    }
    EndRenderingIfActive();
    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    vkCmdPipelineBarrier(static_cast<VkCommandBuffer>(m_commandBuffer), srcStage, dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}
void CommandListVK::BindPixelPackBufferAsSSBO(void* pboHandle, uint32_t slot, size_t size) { (void)pboHandle; (void)slot; (void)size; }

void CommandListVK::PushGraphicsConstantsIfPossible() {
    if (!m_isRecording || !m_commandBuffer || !m_device || !m_currentPipeline) return;
    PipelineVK* pVK = static_cast<PipelineVK*>(m_currentPipeline);
    if (pVK->m_isCompute) return;
    VkPipelineLayout layout = static_cast<VkPipelineLayout>(pVK->m_nativePipelineLayout);
    if (layout == VK_NULL_HANDLE)
        layout = static_cast<VkPipelineLayout>(m_device->GetDefaultPipelineLayout());
    if (layout == VK_NULL_HANDLE) return;
    vkCmdPushConstants(static_cast<VkCommandBuffer>(m_commandBuffer), layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 256, m_pushConstants);
}

void CommandListVK::BindPipeline(IPipeline* pipeline) {
    if (!pipeline || !m_isRecording || !m_commandBuffer || !m_device) return;
    PipelineVK* pVK = static_cast<PipelineVK*>(pipeline);
    VkPipeline vkPipe = static_cast<VkPipeline>(pVK->m_pipeline);
    if (vkPipe == VK_NULL_HANDLE) {
        // Do not set m_currentPipeline: BindDescriptorSets and draws require a real VkPipeline bind.
        m_currentPipeline = nullptr;
        return;
    }
    m_currentPipeline = pipeline;
    m_pushModelSet = false;
    VkPipelineBindPoint bindPoint = pVK->m_isCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindPipeline(static_cast<VkCommandBuffer>(m_commandBuffer), bindPoint, vkPipe);
    if (!pVK->m_isCompute && m_device->SupportsExtendedDynamicState()) {
        VkCommandBuffer cb = static_cast<VkCommandBuffer>(m_commandBuffer);
        vkCmdSetDepthTestEnable(cb, m_depthTestEnabled ? VK_TRUE : VK_FALSE);
        vkCmdSetDepthWriteEnable(cb, m_depthWriteEnabled ? VK_TRUE : VK_FALSE);
        vkCmdSetDepthCompareOp(cb, m_depthFuncLess ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_LESS_OR_EQUAL);
        VkCullModeFlags mode = VK_CULL_MODE_NONE;
        if (m_cullFaceEnabled)
            mode = m_cullFrontFace ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
        vkCmdSetCullMode(cb, mode);
    }
    // Push at bind for immediate validity; draws also push to capture post-bind SetUniform* updates.
    PushGraphicsConstantsIfPossible();
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
    // Keep backend behavior consistent: fullscreen draws always bind the provided pipeline.
    BindPipeline(pipeline);
    BindVertexBuffer(vbo, 0, 0);
    BindIndexBuffer(ibo, IndexType::UInt16, 0);
    PipelineVK* pVK = static_cast<PipelineVK*>(pipeline);
    VkPipelineLayout layout = static_cast<VkPipelineLayout>(pVK->m_nativePipelineLayout ? pVK->m_nativePipelineLayout : m_device->GetDefaultPipelineLayout());
    if (layout != VK_NULL_HANDLE) {
        uint32_t pushSize = 256u;  // matches VulkanResourceSpaces kPushConstantSize
        vkCmdPushConstants(static_cast<VkCommandBuffer>(m_commandBuffer), layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, pushSize, m_pushConstants);
    }
    if (!m_isRenderingActive) {
        core::Log::Warn("DrawFullscreenQuad: not inside active rendering (no render target); draw skipped - output will not be written.");
        return;
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

void CommandListVK::BindDescriptorSets(uint32_t firstSet, uint32_t count, IDescriptorSet* const* sets, const uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount) {
    // Requires a successfully bound graphics/compute pipeline (vkCmdBindPipeline). IPipeline without VkPipeline must not exist (CreateGraphicsPipelineVK enforces that).
    if (count == 0 || !sets || !m_isRecording || !m_commandBuffer || !m_currentPipeline) return;
    PipelineVK* pVK = static_cast<PipelineVK*>(m_currentPipeline);
    VkPipelineLayout layout = static_cast<VkPipelineLayout>(pVK->m_nativePipelineLayout);
    if (layout == VK_NULL_HANDLE) return;
    if (pVK->m_requiredSetCount != 0 && firstSet == 0 && count < pVK->m_requiredSetCount) {
        core::Log::ErrorF("CommandListVK::BindDescriptorSets insufficient set count: layoutKind=%s required=%u provided=%u firstSet=%u",
                          PipelineLayoutKindName(pVK->m_layoutKind), static_cast<uint32_t>(pVK->m_requiredSetCount), count, firstSet);
        return;
    }
    std::vector<VkDescriptorSet> vkSets(count);
    for (uint32_t i = 0; i < count; i++) {
        if (!sets[i]) {
            core::Log::ErrorF("CommandListVK::BindDescriptorSets null descriptor set: layoutKind=%s firstSet=%u localIndex=%u",
                              PipelineLayoutKindName(pVK->m_layoutKind), firstSet, i);
            return;
        }
        DescriptorSetVK* dsVK = static_cast<DescriptorSetVK*>(sets[i]);
        if (!dsVK->IsCompleteForBind(firstSet + i, pVK->m_layoutKind))
            return;
        vkSets[i] = static_cast<VkDescriptorSet>(dsVK->m_descriptorSet);
        if (vkSets[i] == VK_NULL_HANDLE) {
            core::Log::ErrorF("CommandListVK::BindDescriptorSets null vk descriptor set: layoutKind=%s set=%u",
                              PipelineLayoutKindName(pVK->m_layoutKind), firstSet + i);
            return;
        }
    }
    VkPipelineBindPoint bindPoint = pVK->m_isCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindDescriptorSets(static_cast<VkCommandBuffer>(m_commandBuffer), bindPoint, layout, firstSet, count, vkSets.data(), dynamicOffsetCount, dynamicOffsets ? dynamicOffsets : nullptr);
}
void CommandListVK::BindBindlessHeap(IBindlessHeap* heap) {
    // Optional partial update: only Forward and GBuffer pipeline layouts include a dedicated bindless set (see Create*PipelineLayout).
    // Prefer BindDescriptorSets(0, N, ...) with all resource-space sets; this exists for incremental/tooling use after vkCmdBindPipeline.
    if (!m_isRecording || !m_commandBuffer || !m_device || !m_currentPipeline) return;
    PipelineVK* pVK = static_cast<PipelineVK*>(m_currentPipeline);
    if (pVK->m_isCompute) return;
    const uint32_t setIndex = BindlessHeapDescriptorSetIndex(pVK->m_layoutKind);
    if (setIndex == UINT32_MAX) return;
    if (pVK->m_requiredSetCount == 0 || pVK->m_requiredSetCount <= setIndex) return;

    VkPipelineLayout layout = static_cast<VkPipelineLayout>(pVK->m_nativePipelineLayout);
    if (layout == VK_NULL_HANDLE) return;

    VkDescriptorSet vkHeapSet = static_cast<VkDescriptorSet>(m_device->GetEmptyBindlessSet());
    if (heap) {
        BindlessHeapVK* heapVK = static_cast<BindlessHeapVK*>(heap);
        if (heapVK->m_descriptorSet != nullptr)
            vkHeapSet = static_cast<VkDescriptorSet>(heapVK->m_descriptorSet);
    }
    vkCmdBindDescriptorSets(static_cast<VkCommandBuffer>(m_commandBuffer), VK_PIPELINE_BIND_POINT_GRAPHICS, layout, setIndex, 1, &vkHeapSet, 0,
                            nullptr);
}

void CommandListVK::SetPushConstants(uint32_t offset, uint32_t size, const void* data) {
    if (offset + size <= sizeof(m_pushConstants) && data)
        std::memcpy(m_pushConstants + offset, data, size);
}

void CommandListVK::SetUniformInt(const char* name, int value) {
    (void)name;
    (void)value;
}
void CommandListVK::SetUniformUint(const char* name, uint32_t value) {
    (void)name;
    (void)value;
}
void CommandListVK::SetUniformFloat(const char* name, float value) {
    (void)name;
    (void)value;
}
void CommandListVK::SetUniformVec2(const char* name, const float* data) {
    (void)name;
    (void)data;
}
void CommandListVK::SetUniformVec3(const char* name, const float* data) { (void)name; (void)data; }
void CommandListVK::SetUniformVec4(const char* name, const float* data) { (void)name; (void)data; }
void CommandListVK::SetUniformMat4(const char* name, const float* data) {
    (void)name;
    (void)data;
}
void CommandListVK::SetUniformMat4Array(const char* name, const float* data, size_t count) { (void)name; (void)data; (void)count; }
void CommandListVK::SetUniformHandle64(const char* name, uint64_t handle) { (void)name; (void)handle; }
void CommandListVK::SetSubroutineIndices(ShaderStage stage, uint32_t count, const uint32_t* indices) { (void)stage; (void)count; (void)indices; }

void CommandListVK::Draw(uint32_t vertexCount, uint32_t firstVertex) {
    if (!m_isRecording || !m_commandBuffer) return;
    if (!m_isRenderingActive) {
        static int n = 0;
        if (n++ < 4)
            core::Log::WarnF("Vulkan Draw: skipped (no active dynamic rendering); SetRenderTarget did not begin rendering");
        return;
    }
    PushGraphicsConstantsIfPossible();
    vkCmdDraw(static_cast<VkCommandBuffer>(m_commandBuffer), vertexCount, 1, firstVertex, 0);
}
void CommandListVK::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    if (!m_isRecording || !m_commandBuffer) return;
    if (!m_isRenderingActive) {
        static int n = 0;
        if (n++ < 4)
            core::Log::WarnF("Vulkan DrawIndexed: skipped (no active dynamic rendering); SetRenderTarget may have failed");
        return;
    }
    PushGraphicsConstantsIfPossible();
    vkCmdDrawIndexed(static_cast<VkCommandBuffer>(m_commandBuffer), indexCount, 1, firstIndex, vertexOffset, 0);
}
void CommandListVK::DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) {
    DrawIndexed(indexCount, firstIndex, vertexOffset);
}
void CommandListVK::DrawIndexedInstanced(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t instanceCount) {
    if (!m_isRecording || !m_commandBuffer) return;
    if (!m_isRenderingActive) {
        static int n = 0;
        if (n++ < 4)
            core::Log::WarnF("Vulkan DrawIndexedInstanced: skipped (no active dynamic rendering)");
        return;
    }
    PushGraphicsConstantsIfPossible();
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
        VkPipelineLayout computeLayout = static_cast<VkPipelineLayout>(pVK->m_layoutKind != PipelineLayoutKind::PushOnly ? pVK->m_nativePipelineLayout : m_device->GetComputePipelineLayout());
        if (computeLayout != VK_NULL_HANDLE) {
            if (pVK->m_layoutKind == PipelineLayoutKind::PushOnly) {
                m_device->UpdateComputeSet0(this);
                VkDescriptorSet computeSet0 = static_cast<VkDescriptorSet>(m_device->GetComputeSet0());
                VkDescriptorSet emptyBindless = static_cast<VkDescriptorSet>(m_device->GetEmptyBindlessSet());
                VkDescriptorSet sets[2] = { computeSet0, emptyBindless };
                vkCmdBindDescriptorSets(static_cast<VkCommandBuffer>(m_commandBuffer), VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 2, sets, 0, nullptr);
                vkCmdPushConstants(static_cast<VkCommandBuffer>(m_commandBuffer), computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 16, m_pushConstants);
            } else {
                vkCmdPushConstants(static_cast<VkCommandBuffer>(m_commandBuffer), computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 256, m_pushConstants);
            }
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

void CommandListVK::CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) {
    if (!m_isRecording || !m_commandBuffer || !src || !dst || size == 0) return;
    BufferVK* srcVK = static_cast<BufferVK*>(src);
    BufferVK* dstVK = static_cast<BufferVK*>(dst);
    if (!srcVK->m_buffer || !dstVK->m_buffer) return;
    VkBufferCopy region = {};
    region.srcOffset = srcOffset;
    region.dstOffset = dstOffset;
    region.size = size;
    vkCmdCopyBuffer(static_cast<VkCommandBuffer>(m_commandBuffer),
                    static_cast<VkBuffer>(srcVK->m_buffer),
                    static_cast<VkBuffer>(dstVK->m_buffer),
                    1, &region);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
