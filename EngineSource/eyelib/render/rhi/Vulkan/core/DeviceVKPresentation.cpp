#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/barriers/BarrierTrackerVK.h"
#include "eyelib/render/rhi/Vulkan/commands/CommandListVK.h"
#include "eyelib/render/rhi/Vulkan/resources/RenderTargetVK.h"
#include "eyelib/render/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/core/Log.h"

#include <vulkan/vulkan.h>
#include <algorithm>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

void DeviceVK::Present() {
    if (!m_queues.GetGraphicsQueue()) return;
    VkQueue queue = static_cast<VkQueue>(m_queues.GetGraphicsQueue());
    if (!m_pendingPresent) {
        static bool once = false;
        if (!once) {
            core::Log::Warn("Vulkan Present: skipped (no swapchain image acquired this frame).");
            once = true;
        }
        return;
    }
    FrameContextVK& frame = m_frameContexts[m_currentFrameIndex];
    const uint32_t imageIndex = frame.swapchainImageIndex;
    void* renderFinishedPerImage = m_swapchain.GetRenderFinishedSemaphore(imageIndex);
    int presentRes = m_swapchain.Present(queue, renderFinishedPerImage ? renderFinishedPerImage : frame.renderFinished, imageIndex);
    m_pendingPresent = false;
    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR) {
        m_swapchain.Resize(m_swapchain.GetWidth(), m_swapchain.GetHeight(), true);
    }
}

void DeviceVK::OnWindowResized(uint32_t width, uint32_t height) {
    if (!m_device || !m_swapchain.surface) return;
    m_swapchain.Resize(width, height);
}

void DeviceVK::Finish() {
    if (m_queues.GetGraphicsQueue())
        vkQueueWaitIdle(static_cast<VkQueue>(m_queues.GetGraphicsQueue()));
}

void DeviceVK::EnsureImGuiDescriptorPool() {
    if (m_imGuiDescriptorPool != nullptr) return;
    VkDevice device = static_cast<VkDevice>(m_device);
    if (device == VK_NULL_HANDLE) return;
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    };
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = poolSizes;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) == VK_SUCCESS)
        m_imGuiDescriptorPool = pool;
}

void DeviceVK::SetPostBlitCallback(std::function<void(void*)> cb) {
    m_postBlitCallback = std::move(cb);
}

bool DeviceVK::GetImGuiVulkanInitInfo(ImGuiVulkanInitInfo* out) {
    if (!out || !m_device || !m_swapchain.swapchain) return false;
    EnsureImGuiDescriptorPool();
    if (m_imGuiDescriptorPool == nullptr || m_swapchain.GetRenderPass() == nullptr) return false;
    out->instance = GetInstance();
    out->physical_device = GetPhysicalDevice();
    out->device = m_device;
    out->queue_family = m_queues.GetGraphicsQueueFamily();
    out->queue = m_queues.GetGraphicsQueue();
    out->pipeline_cache = m_pipelineCache;
    out->descriptor_pool = m_imGuiDescriptorPool;
    out->min_image_count = 2;
    out->image_count = m_swapchain.GetImageCount();
    out->render_pass = m_swapchain.GetRenderPass();
    return true;
}

void DeviceVK::BlitToDefaultFramebuffer(IRenderTarget* src) {
    if (!m_swapchain.swapchain || m_swapchain.GetImageCount() == 0 || !m_immediateCommandList) {
        static bool once = false;
        if (!once) {
            core::Log::Warn("Vulkan present: BlitToDefaultFramebuffer skipped — no swapchain, zero images, or no immediate command list.");
            once = true;
        }
        return;
    }
    if (!m_pendingPresent) return;  // No swapchain image this frame (Acquire in BeginFrame); only record when we have one.
    FrameContextVK& frame = m_frameContexts[m_currentFrameIndex];
    const uint32_t imageIndex = frame.swapchainImageIndex;
    if (imageIndex >= m_swapchain.GetImageCount()) {
        static bool once = false;
        if (!once) {
            core::Log::WarnF("Vulkan present: imageIndex %u >= count %u.", imageIndex, m_swapchain.GetImageCount());
            once = true;
        }
        return;
    }

    if (!src) {
        // Clear swapchain image then overlay with LOAD pass so ImGui uses the same pass as blit path (single init).
        void* rpHandle = m_swapchain.GetRenderPass();
        void* fbHandle = m_swapchain.GetFramebuffer(imageIndex);
        if (!rpHandle || !fbHandle) {
            static bool logged = false;
            if (!logged) {
                core::Log::Warn("Vulkan present: overlay pass or framebuffer missing; skipping compose.");
                logged = true;
            }
            return;
        }
        CommandListVK* rec = GetRecordingCommandListVK();
        rec->EndRenderingIfActive();
        if (!rec->IsRecording())
            rec->Begin();
        VkCommandBuffer cmd = static_cast<VkCommandBuffer>(rec->GetCommandBuffer());
        if (!rec->IsRecording() || cmd == VK_NULL_HANDLE) {
            static bool once = false;
            if (!once) {
                core::Log::Warn("Vulkan present: null-src path skipped — command buffer not recording or null.");
                once = true;
            }
            return;
        }
        VkImage dstImage = static_cast<VkImage>(m_swapchain.GetImage(imageIndex));
        const VkExtent2D extent = { m_swapchain.GetWidth(), m_swapchain.GetHeight() };
        // Clear image so content is defined (LOAD pass will then load this).
        VkImageMemoryBarrier toDst = {};
        toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toDst.image = dstImage;
        toDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);
        VkClearColorValue clearColor = {};
#if defined(OPENEYE_VULKAN_DEBUG_CLEAR_RED) && OPENEYE_VULKAN_DEBUG_CLEAR_RED
        // Debug: bright red to verify swapchain output is visible (see documentation/vulkan/vulkan_check.md).
        clearColor.float32[0] = 1.0f;
        clearColor.float32[1] = 0.0f;
        clearColor.float32[2] = 0.0f;
        clearColor.float32[3] = 1.0f;
#else
        clearColor.float32[0] = 0.12f;
        clearColor.float32[1] = 0.12f;
        clearColor.float32[2] = 0.15f;
        clearColor.float32[3] = 1.0f;
#endif
        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(cmd, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
        VkImageMemoryBarrier toColor = {};
        toColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toColor.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toColor.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toColor.image = dstImage;
        toColor.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &toColor);
        VkRenderPassBeginInfo rpBegin = {};
        rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass = static_cast<VkRenderPass>(rpHandle);
        rpBegin.framebuffer = static_cast<VkFramebuffer>(fbHandle);
        rpBegin.renderArea = { { 0, 0 }, extent };
        rpBegin.clearValueCount = 0;
        rpBegin.pClearValues = nullptr;
        vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        if (m_postBlitCallback)
            m_postBlitCallback(cmd);
        vkCmdEndRenderPass(cmd);
        return;
    }
    RenderTargetVK* rt = static_cast<RenderTargetVK*>(src);
    ITexture* color0 = rt->GetColorTexture(0);
    if (!color0) return;
    TextureVK* tex = static_cast<TextureVK*>(color0);
    VkImage srcImage = static_cast<VkImage>(tex->m_image);
    if (srcImage == VK_NULL_HANDLE) return;
    CommandListVK* rec = GetRecordingCommandListVK();
    rec->EndRenderingIfActive();
    if (!rec->IsRecording())
        rec->Begin();
    VkCommandBuffer cmd = static_cast<VkCommandBuffer>(rec->GetCommandBuffer());
    if (!rec->IsRecording() || cmd == VK_NULL_HANDLE) return;
    VkImage dstImage = static_cast<VkImage>(m_swapchain.GetImage(imageIndex));
    uint32_t srcW = tex->m_width;
    uint32_t srcH = tex->m_height;
    uint32_t w = std::min(srcW, m_swapchain.GetWidth());
    uint32_t h = std::min(srcH, m_swapchain.GetHeight());
    if (w == 0 || h == 0) return;

    ResourceState srcTracked = BarrierTrackerVK::GetImageState(&m_barrierTracker, tex->m_image);
    VkImageLayout srcOldLayout;
    VkAccessFlags srcOldAccess;
    VkPipelineStageFlags srcStage;
    if (srcTracked == ResourceState::ShaderResource) {
        srcOldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        srcOldAccess = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        srcOldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        srcOldAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    VkImageMemoryBarrier barrierSrc = {};
    barrierSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierSrc.srcAccessMask = srcOldAccess;
    barrierSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrierSrc.oldLayout = srcOldLayout;
    barrierSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrierSrc.image = srcImage;
    barrierSrc.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierSrc);
    VkImageMemoryBarrier barrierDst = {};
    barrierDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrierDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierDst.image = dstImage;
    barrierDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierDst);
    VkImageCopy copy = {};
    copy.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copy.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copy.extent = { w, h, 1 };
    vkCmdCopyImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier restoreSrc = {};
    restoreSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    restoreSrc.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    restoreSrc.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    restoreSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    restoreSrc.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    restoreSrc.image = srcImage;
    restoreSrc.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &restoreSrc);
    BarrierTrackerVK::SetImageState(&m_barrierTracker, tex->m_image, ResourceState::RenderTarget);

    if (m_postBlitCallback && m_swapchain.GetRenderPass() != nullptr && m_swapchain.GetFramebuffer(imageIndex) != nullptr) {
        barrierDst.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierDst.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrierDst.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierDst.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierDst);
        VkRenderPass rp = static_cast<VkRenderPass>(m_swapchain.GetRenderPass());
        VkFramebuffer fb = static_cast<VkFramebuffer>(m_swapchain.GetFramebuffer(imageIndex));
        VkClearValue clearVal = {};
        VkRenderPassBeginInfo rpBegin = {};
        rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass = rp;
        rpBegin.framebuffer = fb;
        rpBegin.renderArea = { { 0, 0 }, { m_swapchain.GetWidth(), m_swapchain.GetHeight() } };
        rpBegin.clearValueCount = 1;
        rpBegin.pClearValues = &clearVal;
        vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        m_postBlitCallback(cmd);
        vkCmdEndRenderPass(cmd);
    } else {
        barrierDst.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierDst.dstAccessMask = 0;
        barrierDst.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierDst.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierDst);
    }
}

void DeviceVK::BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) { (void)nativeFbo; (void)width; (void)height; }

void DeviceVK::BlitToDefaultFramebufferSubregion(IRenderTarget* src, int dstX, int dstY, int dstW, int dstH) { (void)src; (void)dstX; (void)dstY; (void)dstW; (void)dstH; }

void DeviceVK::BlitToDefaultFramebufferLetterbox(IRenderTarget* src, uint32_t windowWidth, uint32_t windowHeight) {
    (void)windowWidth;
    (void)windowHeight;
    BlitToDefaultFramebuffer(src);
}

void DeviceVK::Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) {
    if (!src || !dst || !m_immediateCommandList) return; // immediate wrapper always exists
    RenderTargetVK* srcRT = static_cast<RenderTargetVK*>(src);
    RenderTargetVK* dstRT = static_cast<RenderTargetVK*>(dst);
    ITexture* srcColor = srcRT->GetColorTexture(0);
    ITexture* dstColor = dstRT->GetColorTexture(0);
    if (!srcColor || !dstColor) return;
    TextureVK* srcTex = static_cast<TextureVK*>(srcColor);
    TextureVK* dstTex = static_cast<TextureVK*>(dstColor);
    VkImage srcImage = static_cast<VkImage>(srcTex->m_image);
    VkImage dstImage = static_cast<VkImage>(dstTex->m_image);
    if (srcImage == VK_NULL_HANDLE || dstImage == VK_NULL_HANDLE) return;

    CommandListVK* rec = GetRecordingCommandListVK();
    rec->EndRenderingIfActive();
    if (!rec->IsRecording())
        rec->Begin();
    VkCommandBuffer cmd = static_cast<VkCommandBuffer>(rec->GetCommandBuffer());
    if (!rec->IsRecording() || cmd == VK_NULL_HANDLE) return;

    const uint32_t w = std::min(srcTex->m_width, dstTex->m_width);
    const uint32_t h = std::min(srcTex->m_height, dstTex->m_height);
    if (w == 0 || h == 0) return;

    auto stateToLayoutInfo = [](ResourceState state, VkImageLayout& outLayout, VkAccessFlags& outAccess, VkPipelineStageFlags& outStage) {
        switch (state) {
        case ResourceState::ShaderResource:
            outLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            outAccess = VK_ACCESS_SHADER_READ_BIT;
            outStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        case ResourceState::TransferSrc:
            outLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            outAccess = VK_ACCESS_TRANSFER_READ_BIT;
            outStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case ResourceState::TransferDst:
            outLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            outAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
            outStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case ResourceState::Undefined:
            outLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            outAccess = 0;
            outStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
        case ResourceState::RenderTarget:
        default:
            outLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            outAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            outStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        }
    };

    const ResourceState srcTracked = BarrierTrackerVK::GetImageState(&m_barrierTracker, srcTex->m_image);
    const ResourceState dstTracked = BarrierTrackerVK::GetImageState(&m_barrierTracker, dstTex->m_image);
    VkImageLayout srcOldLayout, dstOldLayout;
    VkAccessFlags srcOldAccess, dstOldAccess;
    VkPipelineStageFlags srcStage, dstStage;
    stateToLayoutInfo(srcTracked, srcOldLayout, srcOldAccess, srcStage);
    stateToLayoutInfo(dstTracked, dstOldLayout, dstOldAccess, dstStage);

    VkImageMemoryBarrier toCopy[2] = {};
    toCopy[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toCopy[0].srcAccessMask = srcOldAccess;
    toCopy[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toCopy[0].oldLayout = srcOldLayout;
    toCopy[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toCopy[0].image = srcImage;
    toCopy[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    toCopy[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toCopy[1].srcAccessMask = dstOldAccess;
    toCopy[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toCopy[1].oldLayout = dstOldLayout;
    toCopy[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toCopy[1].image = dstImage;
    toCopy[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, srcStage | dstStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, toCopy);

    VkImageCopy copy = {};
    copy.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copy.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copy.extent = { w, h, 1 };
    vkCmdCopyImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    const ResourceState srcRestoreState = (srcTracked == ResourceState::Undefined) ? ResourceState::RenderTarget : srcTracked;
    // Blit destination is consumed as sampled input in post/outline flows.
    // Restore to ShaderResource deterministically to avoid layout mismatch with descriptor imageLayout.
    const ResourceState dstRestoreState = ResourceState::ShaderResource;
    VkImageLayout srcRestoreLayout, dstRestoreLayout;
    VkAccessFlags srcRestoreAccess, dstRestoreAccess;
    VkPipelineStageFlags srcRestoreStage, dstRestoreStage;
    stateToLayoutInfo(srcRestoreState, srcRestoreLayout, srcRestoreAccess, srcRestoreStage);
    stateToLayoutInfo(dstRestoreState, dstRestoreLayout, dstRestoreAccess, dstRestoreStage);

    VkImageMemoryBarrier restore[2] = {};
    restore[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    restore[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    restore[0].dstAccessMask = srcRestoreAccess;
    restore[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    restore[0].newLayout = srcRestoreLayout;
    restore[0].image = srcImage;
    restore[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    restore[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    restore[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    restore[1].dstAccessMask = dstRestoreAccess;
    restore[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    restore[1].newLayout = dstRestoreLayout;
    restore[1].image = dstImage;
    restore[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, srcRestoreStage | dstRestoreStage, 0, 0, nullptr, 0, nullptr, 2, restore);

    BarrierTrackerVK::SetImageState(&m_barrierTracker, srcTex->m_image, srcRestoreState);
    BarrierTrackerVK::SetImageState(&m_barrierTracker, dstTex->m_image, dstRestoreState);
    if (!colorOnly) {
        // Depth copy path is intentionally separate and explicit.
        BlitDepthOnly(src, dst);
    }
}

void DeviceVK::BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) { (void)nativeFbo; (void)dst; (void)colorOnly; }

void DeviceVK::BlitDepthOnly(IRenderTarget* src, IRenderTarget* dst) {
    if (!src || !dst || !m_immediateCommandList) return;
    RenderTargetVK* srcRT = static_cast<RenderTargetVK*>(src);
    RenderTargetVK* dstRT = static_cast<RenderTargetVK*>(dst);
    ITexture* srcDepth = srcRT->GetDepthTexture();
    ITexture* dstDepth = dstRT->GetDepthTexture();
    if (!srcDepth || !dstDepth) {
        static bool s_loggedMissingDepth = false;
        if (!s_loggedMissingDepth) {
            core::Log::WarnF("DeviceVK::BlitDepthOnly skipped: srcDepth=%p dstDepth=%p", (void*)srcDepth, (void*)dstDepth);
            s_loggedMissingDepth = true;
        }
        return;
    }

    TextureVK* srcTex = static_cast<TextureVK*>(srcDepth);
    TextureVK* dstTex = static_cast<TextureVK*>(dstDepth);
    VkImage srcImage = static_cast<VkImage>(srcTex->m_image);
    VkImage dstImage = static_cast<VkImage>(dstTex->m_image);
    if (srcImage == VK_NULL_HANDLE || dstImage == VK_NULL_HANDLE) return;

    CommandListVK* rec = GetRecordingCommandListVK();
    rec->EndRenderingIfActive();
    if (!rec->IsRecording())
        rec->Begin();
    VkCommandBuffer cmd = static_cast<VkCommandBuffer>(rec->GetCommandBuffer());
    if (!rec->IsRecording() || cmd == VK_NULL_HANDLE) return;

    const uint32_t w = std::min(srcTex->m_width, dstTex->m_width);
    const uint32_t h = std::min(srcTex->m_height, dstTex->m_height);
    if (w == 0 || h == 0) return;

    auto stateToDepthLayoutInfo = [](ResourceState state, VkImageLayout& outLayout, VkAccessFlags& outAccess, VkPipelineStageFlags& outStage) {
        switch (state) {
        case ResourceState::ShaderResource:
            outLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            outAccess = VK_ACCESS_SHADER_READ_BIT;
            outStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        case ResourceState::DepthRead:
            outLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            outAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            outStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        case ResourceState::DepthWrite:
            outLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            outAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            outStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        case ResourceState::TransferSrc:
            outLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            outAccess = VK_ACCESS_TRANSFER_READ_BIT;
            outStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case ResourceState::TransferDst:
            outLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            outAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
            outStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case ResourceState::Undefined:
            outLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            outAccess = 0;
            outStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
        default:
            outLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            outAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            outStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        }
    };

    const ResourceState srcTracked = BarrierTrackerVK::GetImageState(&m_barrierTracker, srcTex->m_image);
    const ResourceState dstTracked = BarrierTrackerVK::GetImageState(&m_barrierTracker, dstTex->m_image);
    VkImageLayout srcOldLayout, dstOldLayout;
    VkAccessFlags srcOldAccess, dstOldAccess;
    VkPipelineStageFlags srcStage, dstStage;
    stateToDepthLayoutInfo(srcTracked, srcOldLayout, srcOldAccess, srcStage);
    stateToDepthLayoutInfo(dstTracked, dstOldLayout, dstOldAccess, dstStage);

    VkImageMemoryBarrier toCopy[2] = {};
    toCopy[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toCopy[0].srcAccessMask = srcOldAccess;
    toCopy[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toCopy[0].oldLayout = srcOldLayout;
    toCopy[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toCopy[0].image = srcImage;
    toCopy[0].subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    toCopy[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toCopy[1].srcAccessMask = dstOldAccess;
    toCopy[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toCopy[1].oldLayout = dstOldLayout;
    toCopy[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toCopy[1].image = dstImage;
    toCopy[1].subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, srcStage | dstStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, toCopy);

    VkImageCopy copy = {};
    copy.srcSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1 };
    copy.dstSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1 };
    copy.extent = { w, h, 1 };
    vkCmdCopyImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    const ResourceState srcRestoreState = (srcTracked == ResourceState::Undefined) ? ResourceState::DepthWrite : srcTracked;
    const ResourceState dstRestoreState = (dstTracked == ResourceState::Undefined) ? ResourceState::DepthWrite : dstTracked;
    VkImageLayout srcRestoreLayout, dstRestoreLayout;
    VkAccessFlags srcRestoreAccess, dstRestoreAccess;
    VkPipelineStageFlags srcRestoreStage, dstRestoreStage;
    stateToDepthLayoutInfo(srcRestoreState, srcRestoreLayout, srcRestoreAccess, srcRestoreStage);
    stateToDepthLayoutInfo(dstRestoreState, dstRestoreLayout, dstRestoreAccess, dstRestoreStage);

    VkImageMemoryBarrier restore[2] = {};
    restore[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    restore[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    restore[0].dstAccessMask = srcRestoreAccess;
    restore[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    restore[0].newLayout = srcRestoreLayout;
    restore[0].image = srcImage;
    restore[0].subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    restore[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    restore[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    restore[1].dstAccessMask = dstRestoreAccess;
    restore[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    restore[1].newLayout = dstRestoreLayout;
    restore[1].image = dstImage;
    restore[1].subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, srcRestoreStage | dstRestoreStage, 0, 0, nullptr, 0, nullptr, 2, restore);

    BarrierTrackerVK::SetImageState(&m_barrierTracker, srcTex->m_image, srcRestoreState);
    BarrierTrackerVK::SetImageState(&m_barrierTracker, dstTex->m_image, dstRestoreState);
}

bool DeviceVK::ReadPixelUint32(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t* outValue) { (void)rt; (void)x; (void)y; (void)outValue; return false; }

bool DeviceVK::ReadPixelsRGBA8(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t* outBuffer) { (void)rt; (void)x; (void)y; (void)w; (void)h; (void)outBuffer; return false; }

void* DeviceVK::CreatePixelPackBuffer(size_t sizeInBytes) { (void)sizeInBytes; return nullptr; }

void DeviceVK::DestroyPixelPackBuffer(void* handle) { (void)handle; }

void DeviceVK::ReadPixelsToPBO(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, void* pboHandle) { (void)rt; (void)x; (void)y; (void)w; (void)h; (void)pboHandle; }

void DeviceVK::MapPixelPackBuffer(void* pboHandle, void* outBuffer, size_t sizeInBytes, uint32_t width, uint32_t height) { (void)pboHandle; (void)outBuffer; (void)sizeInBytes; (void)width; (void)height; }

void DeviceVK::MapPixelPackBufferRaw(void* pboHandle, void* outBuffer, size_t sizeInBytes) { (void)pboHandle; (void)outBuffer; (void)sizeInBytes; }

} // namespace rhi
} // namespace render
} // namespace eyelib
