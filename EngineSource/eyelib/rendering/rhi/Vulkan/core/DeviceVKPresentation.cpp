#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/barriers/BarrierTrackerVK.h"
#include "eyelib/rendering/rhi/Vulkan/commands/CommandListVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/RenderTargetVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/core/Log.h"

#include <vulkan/vulkan.h>
#include <algorithm>
#include <vector>

/** Presentation helpers: **`Present`** waits on **per-image** **`renderFinished`** (or frame fallback), **`BlitToDefaultFramebuffer`**
 *  copies or **shader-resolves** scene color to the acquired swapchain image, optional **`m_postBlitCallback`** overlay via **LOAD** render pass,
 *  then transitions to **`PRESENT_SRC`**. **`EnsurePresentResolveResources`** builds **`passes/post/present_resolve`** when HDR and swapchain formats differ.
 *  **`OnWindowResized`** forwards to **`SwapchainVK::Resize`**. */

namespace eyelib {
namespace rendering {
namespace rhi {

namespace {

static void CmdImageBarrier2(VkCommandBuffer cmd, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                             const VkImageMemoryBarrier& b) {
    VkImageMemoryBarrier2 b2 = {};
    b2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    b2.srcStageMask = static_cast<VkPipelineStageFlags2>(srcStage);
    b2.srcAccessMask = static_cast<VkAccessFlags2>(b.srcAccessMask);
    b2.dstStageMask = static_cast<VkPipelineStageFlags2>(dstStage);
    b2.dstAccessMask = static_cast<VkAccessFlags2>(b.dstAccessMask);
    b2.oldLayout = b.oldLayout;
    b2.newLayout = b.newLayout;
    b2.srcQueueFamilyIndex = b.srcQueueFamilyIndex;
    b2.dstQueueFamilyIndex = b.dstQueueFamilyIndex;
    b2.image = b.image;
    b2.subresourceRange = b.subresourceRange;
    VkDependencyInfo dep = {};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &b2;
    vkCmdPipelineBarrier2(cmd, &dep);
}

static void CmdImageBarriers2(VkCommandBuffer cmd, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                              const VkImageMemoryBarrier* barriers, uint32_t count) {
    if (!barriers || count == 0) return;
    std::vector<VkImageMemoryBarrier2> b2;
    b2.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkImageMemoryBarrier2 x = {};
        x.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        x.srcStageMask = static_cast<VkPipelineStageFlags2>(srcStage);
        x.srcAccessMask = static_cast<VkAccessFlags2>(barriers[i].srcAccessMask);
        x.dstStageMask = static_cast<VkPipelineStageFlags2>(dstStage);
        x.dstAccessMask = static_cast<VkAccessFlags2>(barriers[i].dstAccessMask);
        x.oldLayout = barriers[i].oldLayout;
        x.newLayout = barriers[i].newLayout;
        x.srcQueueFamilyIndex = barriers[i].srcQueueFamilyIndex;
        x.dstQueueFamilyIndex = barriers[i].dstQueueFamilyIndex;
        x.image = barriers[i].image;
        x.subresourceRange = barriers[i].subresourceRange;
        b2.push_back(x);
    }
    VkDependencyInfo dep = {};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = count;
    dep.pImageMemoryBarriers = b2.data();
    vkCmdPipelineBarrier2(cmd, &dep);
}

} // namespace

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
    NoteVulkanDeviceLost("Present vkQueuePresentKHR", presentRes);
    m_pendingPresent = false;
    if (presentRes == static_cast<int>(VK_SUCCESS)) {
    } else if (presentRes == static_cast<int>(VK_ERROR_OUT_OF_DATE_KHR) ||
               presentRes == static_cast<int>(VK_SUBOPTIMAL_KHR)) {
        NoteSwapchainKhrResult("Present vkQueuePresentKHR", presentRes, SwapchainKhrPath::Present);
    } else {
        core::Log::ErrorF("Vulkan Present: vkQueuePresentKHR failed (0x%x)", static_cast<unsigned>(presentRes));
    }
}

void DeviceVK::OnWindowResized(uint32_t width, uint32_t height) {
    if (!m_device || !m_swapchain.surface) return;
    m_swapchain.Resize(width, height, false, this, m_swapchainPresentMode);
}

void DeviceVK::Finish() {
    if (m_queues.GetGraphicsQueue()) {
        const VkResult idle = vkQueueWaitIdle(static_cast<VkQueue>(m_queues.GetGraphicsQueue()));
        NoteVulkanDeviceLost("Finish vkQueueWaitIdle", static_cast<int>(idle));
    }
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
        CmdImageBarrier2(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, toDst);
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
        CmdImageBarrier2(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, toColor);
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
        // Swapchain overlay RP uses finalLayout PRESENT_SRC_KHR; keep tracker aligned for acquire/blit next frame.
        BarrierTrackerVK::SetImageState(&m_barrierTracker, static_cast<void*>(dstImage), ResourceState::Present);
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

    const VkFormat srcVkFmt = ToVkFormat(tex->GetFormat());
    const VkFormat dstVkFmt = static_cast<VkFormat>(m_swapchain.format);
    const bool useVkCopy = (srcVkFmt != VK_FORMAT_UNDEFINED && srcVkFmt == dstVkFmt);

    VkImageMemoryBarrier barrierDst = {};
    barrierDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrierDst.image = dstImage;

    VkAccessFlags dstPostComposeAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
    VkImageLayout dstPostComposeLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    VkPipelineStageFlags dstPostComposeSrcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    auto clearSwapchainViaTransfer = [&]() {
        barrierDst.srcAccessMask = 0;
        barrierDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        CmdImageBarrier2(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, barrierDst);
        VkClearColorValue clearColor = {};
        clearColor.float32[0] = 0.12f;
        clearColor.float32[1] = 0.12f;
        clearColor.float32[2] = 0.15f;
        clearColor.float32[3] = 1.0f;
        VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(cmd, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
        dstPostComposeAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstPostComposeLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dstPostComposeSrcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    };

    if (useVkCopy) {
        static bool loggedCopyPath = false;
        if (!loggedCopyPath) {
            core::Log::InfoF("Vulkan present: using vkCmdCopyImage path (format match 0x%x).", static_cast<unsigned>(dstVkFmt));
            loggedCopyPath = true;
        }
        VkImageMemoryBarrier barrierSrc = {};
        barrierSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierSrc.srcAccessMask = srcOldAccess;
        barrierSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrierSrc.oldLayout = srcOldLayout;
        barrierSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrierSrc.image = srcImage;
        barrierSrc.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        CmdImageBarrier2(cmd, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, barrierSrc);
        barrierDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        CmdImageBarrier2(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, barrierDst);
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
        CmdImageBarrier2(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, restoreSrc);
        BarrierTrackerVK::SetImageState(&m_barrierTracker, tex->m_image, ResourceState::RenderTarget);
    } else {
        static bool loggedResolvePath = false;
        if (!loggedResolvePath) {
            core::Log::InfoF("Vulkan present: using present_resolve shader path (src=0x%x dst=0x%x).",
                             static_cast<unsigned>(srcVkFmt), static_cast<unsigned>(dstVkFmt));
            loggedResolvePath = true;
        }
        if (!EnsurePresentResolveResources()) {
            static bool once = false;
            if (!once) {
                core::Log::Warn("Vulkan present: HDR→swapchain resolve skipped (EnsurePresentResolveResources failed).");
                once = true;
            }
            clearSwapchainViaTransfer();
        } else {
            VkImageMemoryBarrier toSample = {};
            toSample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toSample.srcAccessMask = srcOldAccess;
            toSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            toSample.oldLayout = srcOldLayout;
            toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toSample.image = srcImage;
            toSample.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            barrierDst.srcAccessMask = 0;
            barrierDst.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrierDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierDst.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            CmdImageBarrier2(cmd, srcStage | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, toSample);
            CmdImageBarrier2(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, barrierDst);

            void* swapView = m_swapchain.GetImageView(imageIndex);
            bool renderedResolve = false;
            if (swapView) {
                const float clearCol[4] = { 0.12f, 0.12f, 0.15f, 1.f };
                rec->BeginRenderingSwapchainColor(swapView, m_swapchain.GetWidth(), m_swapchain.GetHeight(), true, clearCol);
                rec->SetViewport(0, 0, m_swapchain.GetWidth(), m_swapchain.GetHeight());
                rec->SetDepthTest(false);
                rec->SetDepthWrite(false);
                rec->SetCullFace(false);
                rec->BindPipeline(m_presentResolvePipeline);
                IDescriptorSet* postSet = m_presentResolvePostSets[m_currentFrameIndex % kMaxFrames];
                if (!m_presentResolveFrameSet || !postSet) {
                    static bool once = false;
                    if (!once) {
                        core::Log::Warn("Vulkan present: resolve descriptor set unavailable; presenting clear-color fallback.");
                        once = true;
                    }
                } else {
                    postSet->UpdateTexture(0, color0);
                    IDescriptorSet* sets[2] = { m_presentResolveFrameSet, postSet };
                    rec->BindDescriptorSets(0, 2, sets);
                    rec->DrawFullscreenQuad(m_presentResolvePipeline);
                    renderedResolve = true;
                }
                rec->EndRenderingIfActive();
                (void)renderedResolve;
                dstPostComposeAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dstPostComposeLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                dstPostComposeSrcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            } else {
                static bool once = false;
                if (!once) {
                    core::Log::Warn("Vulkan present: swapchain image view missing; presenting clear-color fallback.");
                    once = true;
                }
                clearSwapchainViaTransfer();
            }

            VkImageMemoryBarrier restoreSrc = {};
            restoreSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            restoreSrc.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            restoreSrc.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            restoreSrc.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            restoreSrc.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            restoreSrc.image = srcImage;
            restoreSrc.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            CmdImageBarrier2(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, restoreSrc);
            BarrierTrackerVK::SetImageState(&m_barrierTracker, tex->m_image, ResourceState::RenderTarget);
        }
    }

    if (m_postBlitCallback && m_swapchain.GetRenderPass() != nullptr && m_swapchain.GetFramebuffer(imageIndex) != nullptr) {
        barrierDst.srcAccessMask = dstPostComposeAccess;
        barrierDst.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrierDst.oldLayout = dstPostComposeLayout;
        barrierDst.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        CmdImageBarrier2(cmd, dstPostComposeSrcStage, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, barrierDst);
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
        BarrierTrackerVK::SetImageState(&m_barrierTracker, static_cast<void*>(dstImage), ResourceState::Present);
    } else {
        barrierDst.srcAccessMask = dstPostComposeAccess;
        barrierDst.dstAccessMask = 0;
        barrierDst.oldLayout = dstPostComposeLayout;
        barrierDst.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        CmdImageBarrier2(cmd, dstPostComposeSrcStage, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, barrierDst);
        BarrierTrackerVK::SetImageState(&m_barrierTracker, static_cast<void*>(dstImage), ResourceState::Present);
    }
}

void DeviceVK::DestroyPresentResolveResources() {
    if (m_presentResolvePipeline) {
        DestroyPipeline(m_presentResolvePipeline);
        m_presentResolvePipeline = nullptr;
    }
    m_presentResolveSwapchainVkFormat = 0;
    if (m_presentResolveFrameSet) {
        DestroyDescriptorSet(m_presentResolveFrameSet);
        m_presentResolveFrameSet = nullptr;
    }
    for (IDescriptorSet*& set : m_presentResolvePostSets) {
        if (set) {
            DestroyDescriptorSet(set);
            set = nullptr;
        }
    }
    if (m_presentResolveFrameUBO) {
        DestroyBuffer(m_presentResolveFrameUBO);
        m_presentResolveFrameUBO = nullptr;
    }
}

bool DeviceVK::EnsurePresentResolveResources() {
    const uint32_t swapFmtU = m_swapchain.format;
    if (swapFmtU == 0)
        return false;
    IPipelineLayout* frameLayoutWr = GetResourceSetLayout(ResourceSetType::Frame);
    IPipelineLayout* postLayoutWr = GetResourceSetLayout(ResourceSetType::PostPass);
    if (!frameLayoutWr || !postLayoutWr)
        return false;

    if (!m_presentResolveFrameUBO) {
        BufferDesc bd = {};
        bd.sizeInBytes = 256;
        bd.usage = BufferUsage::Uniform;
        bd.memoryHint = BufferMemoryHint::HostVisible;
        m_presentResolveFrameUBO = CreateBuffer(bd, nullptr);
        if (!m_presentResolveFrameUBO)
            return false;
    }
    if (!m_presentResolveFrameSet) {
        m_presentResolveFrameSet = CreateDescriptorSet(frameLayoutWr);
        if (!m_presentResolveFrameSet)
            return false;
        m_presentResolveFrameSet->UpdateUniformBuffer(0, m_presentResolveFrameUBO, 0, 128);
        m_presentResolveFrameSet->UpdateUniformBuffer(1, m_presentResolveFrameUBO, 128, 128);
    }
    for (uint32_t i = 0; i < kMaxFrames; ++i) {
        if (m_presentResolvePostSets[i])
            continue;
        m_presentResolvePostSets[i] = CreateDescriptorSet(postLayoutWr);
        if (!m_presentResolvePostSets[i])
            return false;
        for (uint32_t b = 0; b < 7; ++b)
            m_presentResolvePostSets[i]->UpdateTexture(b, nullptr);
    }

    if (m_presentResolvePipeline && m_presentResolveSwapchainVkFormat == swapFmtU)
        return true;

    if (m_presentResolvePipeline) {
        DestroyPipeline(m_presentResolvePipeline);
        m_presentResolvePipeline = nullptr;
    }

    RHITextureFormat swapColorRhi = RHITextureFormat::BGRA8;
    switch (static_cast<VkFormat>(swapFmtU)) {
    case VK_FORMAT_B8G8R8A8_UNORM:
        swapColorRhi = RHITextureFormat::BGRA8;
        break;
    case VK_FORMAT_R8G8B8A8_UNORM:
        swapColorRhi = RHITextureFormat::RGBA8;
        break;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        swapColorRhi = RHITextureFormat::BGR10A2;
        break;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        swapColorRhi = RHITextureFormat::RGBA16F;
        break;
    default: {
        static bool once = false;
        if (!once) {
            core::Log::WarnF("Vulkan present: swapchain format 0x%x has no present_resolve mapping; extend DeviceVK::EnsurePresentResolveResources.",
                              static_cast<unsigned>(swapFmtU));
            once = true;
        }
        return false;
    }
    }

    VertexLayout vl = {};
    vl.strideInBytes = 4u * sizeof(float);
    vl.attributes = {
        {0u, VertexAttributeType::Float2, 0u},
        {1u, VertexAttributeType::Float2, 2u * sizeof(float)},
    };

    ShaderGraphicsPipelineCreateInfo info = {};
    info.assetPath = "passes/post/present_resolve";
    info.vertexLayout = vl;
    info.topology = PrimitiveTopology::TriangleList;
    info.layoutKind = PipelineLayoutKind::Post;
    info.useDepthStencilOverride = true;
    info.depthStencilOverride.depthTest = false;
    info.depthStencilOverride.depthWrite = false;
    info.sourcePolicy = ShaderSourcePolicy::CompileFromSource;
    PipelineAttachmentFormats af;
    af.color = { swapColorRhi };
    af.depthStencil = RHITextureFormat::None;
    info.attachmentFormats = af;

    m_presentResolvePipeline = CreateGraphicsPipelineFromShaderAsset(info);
    if (!m_presentResolvePipeline)
        return false;
    m_presentResolveSwapchainVkFormat = swapFmtU;
    return true;
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
    CmdImageBarriers2(cmd, srcStage | dstStage, VK_PIPELINE_STAGE_TRANSFER_BIT, toCopy, 2);

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
    CmdImageBarriers2(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, srcRestoreStage | dstRestoreStage, restore, 2);

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
            outAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
            outStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
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
    CmdImageBarriers2(cmd, srcStage | dstStage, VK_PIPELINE_STAGE_TRANSFER_BIT, toCopy, 2);

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
    CmdImageBarriers2(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, srcRestoreStage | dstRestoreStage, restore, 2);

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
} // namespace rendering
} // namespace eyelib
