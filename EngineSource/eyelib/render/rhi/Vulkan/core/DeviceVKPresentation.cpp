#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
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
            core::Log::Warn("Vulkan Present: skipped (m_pendingPresent false — BlitToDefaultFramebuffer did not run or returned early).");
            once = true;
        }
        return;
    }
    FrameContextVK& frame = m_frameContexts[m_currentFrameIndex];
    m_swapchain.Present(queue, frame.renderFinished);
    m_pendingPresent = false;
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
    FrameContextVK& frame = m_frameContexts[m_currentFrameIndex];
    if (!m_swapchain.Acquire(frame.imageAvailable, UINT64_MAX)) {
        static bool once = false;
        if (!once) {
            core::Log::Warn("Vulkan present: swapchain Acquire failed.");
            once = true;
        }
        return;
    }
    m_pendingPresent = true;
    const uint32_t imageIndex = m_swapchain.GetCurrentImageIndex();
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
        CommandListVK* imm = m_immediateCommandList.get();
        imm->EndRenderingIfActive();
        if (!imm->IsRecording())
            imm->Begin();
        VkCommandBuffer cmd = static_cast<VkCommandBuffer>(imm->GetCommandBuffer());
        if (!imm->IsRecording() || cmd == VK_NULL_HANDLE) {
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
        static int s_clearOverlayFramesLogged = 0;
        if (s_clearOverlayFramesLogged < 3) {
            core::Log::InfoF("Vulkan present: clear+overlay path executed (null src), overlay=%s, frame %d.",
                m_postBlitCallback ? "yes" : "no", s_clearOverlayFramesLogged + 1);
            ++s_clearOverlayFramesLogged;
        }
        return;
    }
    RenderTargetVK* rt = static_cast<RenderTargetVK*>(src);
    ITexture* color0 = rt->GetColorTexture(0);
    if (!color0) return;
    TextureVK* tex = static_cast<TextureVK*>(color0);
    VkImage srcImage = static_cast<VkImage>(tex->m_image);
    if (srcImage == VK_NULL_HANDLE) return;
    CommandListVK* imm = m_immediateCommandList.get();
    imm->EndRenderingIfActive();
    VkCommandBuffer cmd = static_cast<VkCommandBuffer>(imm->GetCommandBuffer());
    if (!imm->IsRecording() || cmd == VK_NULL_HANDLE) return;
    VkImage dstImage = static_cast<VkImage>(m_swapchain.GetImage(imageIndex));
    uint32_t srcW = tex->m_width;
    uint32_t srcH = tex->m_height;
    uint32_t w = std::min(srcW, m_swapchain.GetWidth());
    uint32_t h = std::min(srcH, m_swapchain.GetHeight());
    if (w == 0 || h == 0) return;
    VkImageMemoryBarrier barrierSrc = {};
    barrierSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierSrc.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrierSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrierSrc.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrierSrc.image = srcImage;
    barrierSrc.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierSrc);
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

void DeviceVK::Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) { (void)src; (void)dst; (void)colorOnly; }

void DeviceVK::BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) { (void)nativeFbo; (void)dst; (void)colorOnly; }

void DeviceVK::BlitDepthOnly(IRenderTarget* src, IRenderTarget* dst) { (void)src; (void)dst; }

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
