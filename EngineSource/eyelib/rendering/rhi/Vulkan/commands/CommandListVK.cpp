#include "eyelib/rendering/rhi/Vulkan/commands/CommandListVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/sync/FenceVK.h"
#include "eyelib/rendering/rhi/Vulkan/sync/QueryPoolVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/RenderTargetVK.h"
#include "eyelib/rendering/rhi/Vulkan/descriptors/BindlessHeapVK.h"
#include "eyelib/rendering/rhi/Vulkan/descriptors/DescriptorSetVK.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/PipelineVK.h"
#include "eyelib/rendering/rhi/Vulkan/barriers/BarrierTrackerVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/rendering/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/rendering/backend/RenderBackendPolicy.h"
#include <vulkan/vulkan.h>
#include "eyelib/core/Log.h"
#include <algorithm>
#include <cstdint>
#include <vector>
#include <cstring>
#include <cassert>

#ifndef OPENEYE_RHI_BINDLESS_PROFILE
#define OPENEYE_RHI_BINDLESS_PROFILE 0
#endif
#if OPENEYE_RHI_BINDLESS_PROFILE
#include <chrono>
#endif

namespace eyelib {
namespace rendering {
namespace rhi {

namespace {

static uint32_t TextureVkArrayLayers(const TextureVK* t) {
    if (!t) return 1u;
    switch (t->m_type) {
    case TextureType::TextureCube:
        return 6u;
    case TextureType::Texture2DArray:
        return (std::max)(1u, t->m_depthOrArray);
    default:
        return 1u;
    }
}

static VkAttachmentLoadOp ToVkLoadOp(::eyelib::rendering::AttachmentLoadOp op) {
    switch (op) {
    case ::eyelib::rendering::AttachmentLoadOp::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
    case ::eyelib::rendering::AttachmentLoadOp::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case ::eyelib::rendering::AttachmentLoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    default: return VK_ATTACHMENT_LOAD_OP_LOAD;
    }
}

static VkAttachmentStoreOp ToVkStoreOp(::eyelib::rendering::AttachmentStoreOp op) {
    switch (op) {
    case ::eyelib::rendering::AttachmentStoreOp::Store: return VK_ATTACHMENT_STORE_OP_STORE;
    case ::eyelib::rendering::AttachmentStoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    default: return VK_ATTACHMENT_STORE_OP_STORE;
    }
}

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
    case PipelineLayoutKind::ComposeBootstrap: return "ComposeBootstrap";
    case PipelineLayoutKind::RasterForwardMinimal: return "RasterForwardMinimal";
    case PipelineLayoutKind::RasterGBuffer: return "RasterGBuffer";
    case PipelineLayoutKind::RasterDeferredLighting: return "RasterDeferredLighting";
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

/** `PipelineVK` stores `m_nativePipelineLayout == nullptr` for `PushOnly` compute even though `VkComputePipelineCreateInfo::layout` uses `GetComputePipelineLayout()` (set0 + bindless set1). Match `Dispatch` / push path resolution. */
static VkPipelineLayout ResolvePipelineLayoutForDescriptorBind(DeviceVK* device, PipelineVK* pVK) {
    if (!device || !pVK) return VK_NULL_HANDLE;
    if (pVK->m_isCompute && pVK->m_layoutKind == PipelineLayoutKind::PushOnly)
        return static_cast<VkPipelineLayout>(device->GetComputePipelineLayout());
    return static_cast<VkPipelineLayout>(pVK->m_nativePipelineLayout);
}

/** Bindless heap set index: Forward 3, GBuffer 2, default compute layout (`GetComputePipelineLayout`) set **1**; other compute layouts have no bindless slot here. */
static uint32_t BindlessHeapSetIndexForCurrentPipeline(DeviceVK* device, PipelineVK* pVK) {
    if (!pVK) return UINT32_MAX;
    const VkPipelineLayout layout = ResolvePipelineLayoutForDescriptorBind(device, pVK);
    if (layout == VK_NULL_HANDLE) return UINT32_MAX;
    if (!pVK->m_isCompute)
        return BindlessHeapDescriptorSetIndex(pVK->m_layoutKind);
    if (layout == static_cast<VkPipelineLayout>(device->GetComputePipelineLayout()))
        return 1u;
    return UINT32_MAX;
}

static VkBuffer VkBufferFromRhi(IBuffer* buffer) {
    if (!buffer) return VK_NULL_HANDLE;
    BufferVK* bv = static_cast<BufferVK*>(buffer);
    return bv->m_buffer ? static_cast<VkBuffer>(bv->m_buffer) : VK_NULL_HANDLE;
}

static uint64_t MakeQueryKey(const IQueryPool* pool, uint32_t index) {
    const uint64_t poolHash = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pool));
    return (poolHash << 32) ^ static_cast<uint64_t>(index);
}

#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR)
static VkExtent2D ToVkFragmentShadingRateExtent(FragmentShadingRate rate) {
    switch (rate) {
    case FragmentShadingRate::Rate1x1: return { 1u, 1u };
    case FragmentShadingRate::Rate1x2: return { 1u, 2u };
    case FragmentShadingRate::Rate2x1: return { 2u, 1u };
    case FragmentShadingRate::Rate2x2: return { 2u, 2u };
    case FragmentShadingRate::Rate2x4: return { 2u, 4u };
    case FragmentShadingRate::Rate4x2: return { 4u, 2u };
    case FragmentShadingRate::Rate4x4: return { 4u, 4u };
    default: return { 1u, 1u };
    }
}

static VkFragmentShadingRateCombinerOpKHR ToVkFragmentShadingRateCombinerOp(FragmentShadingRateCombinerOp op) {
    switch (op) {
    case FragmentShadingRateCombinerOp::Keep: return VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
    case FragmentShadingRateCombinerOp::Replace: return VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR;
    case FragmentShadingRateCombinerOp::Min: return VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MIN_KHR;
    case FragmentShadingRateCombinerOp::Max: return VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR;
    case FragmentShadingRateCombinerOp::Mul: return VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MUL_KHR;
    default: return VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
    }
}
#endif

#if OPENEYE_RHI_BINDLESS_PROFILE
enum class BindlessBindProfilePath : uint8_t { DescriptorBuffer, ClassicSet };

static void LogBindlessHeapBindProfile(BindlessBindProfilePath path, int64_t micros) {
    static bool s_firstDb = true;
    static bool s_firstClassic = true;
    static bool s_slowDbWarned = false;
    static bool s_slowClassicWarned = false;
    if (path == BindlessBindProfilePath::DescriptorBuffer && s_firstDb) {
        core::Log::InfoF("CommandListVK::BindBindlessHeap profile: first descriptor-buffer bind path (%lld us).", static_cast<long long>(micros));
        s_firstDb = false;
    }
    if (path == BindlessBindProfilePath::ClassicSet && s_firstClassic) {
        core::Log::InfoF("CommandListVK::BindBindlessHeap profile: first classic vkCmdBindDescriptorSets path (%lld us).", static_cast<long long>(micros));
        s_firstClassic = false;
    }
    constexpr int64_t kSlowUs = 500;
    if (micros >= kSlowUs) {
        if (path == BindlessBindProfilePath::DescriptorBuffer && !s_slowDbWarned) {
            core::Log::WarnF(
                "CommandListVK::BindBindlessHeap profile: slow descriptor-buffer bind (%lld us); further slow-path logs suppressed.",
                static_cast<long long>(micros));
            s_slowDbWarned = true;
        }
        if (path == BindlessBindProfilePath::ClassicSet && !s_slowClassicWarned) {
            core::Log::WarnF(
                "CommandListVK::BindBindlessHeap profile: slow classic bind (%lld us); further slow-path logs suppressed.",
                static_cast<long long>(micros));
            s_slowClassicWarned = true;
        }
    }
}
#endif
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
    m_activeDynamicRenderWidth = 0;
    m_activeDynamicRenderHeight = 0;
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
    m_hasGraphicsWork = false;
    m_hasComputeWork = false;
    m_computeQueueEligible = true;
    m_requiresQueueOwnershipTransfers = false;
    m_crossQueueOwnershipUnsafe = false;
    m_crossQueueBuffers.clear();
    m_crossQueueBufferSet.clear();
    m_crossQueueImages.clear();
    m_activeQueryKeys.clear();
}

void CommandListVK::NoteCrossQueueBuffer(void* vkBuffer) {
    if (!vkBuffer)
        return;
    const uint64_t key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(vkBuffer));
    if (m_crossQueueBufferSet.insert(key).second)
        m_crossQueueBuffers.push_back(vkBuffer);
}

void CommandListVK::NoteCrossQueueImage(void* vkImage, uint32_t aspectMask, uint32_t baseMipLevel, uint32_t levelCount, uint32_t baseArrayLayer,
                                        uint32_t arrayLayerCount, ResourceState ownershipLayout) {
    if (!vkImage || ownershipLayout == ResourceState::Undefined)
        return;
    CrossQueueImageMeta meta = {};
    meta.vkImage = vkImage;
    meta.aspectMask = aspectMask;
    meta.baseMipLevel = baseMipLevel;
    meta.levelCount = levelCount;
    meta.baseArrayLayer = baseArrayLayer;
    meta.arrayLayerCount = arrayLayerCount;
    meta.ownershipLayout = ownershipLayout;
    for (CrossQueueImageMeta& e : m_crossQueueImages) {
        if (e.vkImage == vkImage) {
            e = meta;
            return;
        }
    }
    m_crossQueueImages.push_back(meta);
}

void CommandListVK::Begin() {
    // Compose buffer: begin only after optional editor slices (BeginFrame no longer starts immediate recording).
    if (m_isImmediate && m_device && m_device->IsFrameOpen()) {
        if (m_isRecording)
            return;
        VkCommandBuffer bufImm = static_cast<VkCommandBuffer>(m_commandBuffer);
        if (bufImm == VK_NULL_HANDLE)
            return;
        ResetCpuRecordingStateAfterPoolReset();
        vkResetCommandBuffer(bufImm, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        VkCommandBufferBeginInfo beginInfoImm = {};
        beginInfoImm.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfoImm.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkResult br = vkBeginCommandBuffer(bufImm, &beginInfoImm);
        if (br == VK_SUCCESS)
            m_isRecording = true;
        else
            core::Log::ErrorF("CommandListVK::Begin: vkBeginCommandBuffer failed (immediate, 0x%x)", br);
        return;
    }
    m_isRecording = false;
    // Stateless rule: clear all bound state so no state survives across frames.
    ResetCpuRecordingStateAfterPoolReset();
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(m_commandBuffer);
    if (buf == VK_NULL_HANDLE) return;
    vkResetCommandBuffer(buf, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult br = vkBeginCommandBuffer(buf, &beginInfo);
    if (br == VK_SUCCESS)
        m_isRecording = true;
    else
        core::Log::ErrorF("CommandListVK::Begin: vkBeginCommandBuffer failed (deferred, 0x%x)", br);
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
    m_activeDynamicRenderWidth = 0;
    m_activeDynamicRenderHeight = 0;
}

void CommandListVK::BeginRenderingSwapchainColor(void* colorImageView, uint32_t width, uint32_t height, bool clear,
                                                 const float clearColor[4]) {
    if (!colorImageView || width == 0 || height == 0 || !m_isRecording || !m_commandBuffer || !m_device) return;
    EndRenderingIfActive();
    VkRenderingAttachmentInfo att = {};
    att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    att.imageView = static_cast<VkImageView>(colorImageView);
    att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    att.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.clearValue.color.float32[0] = clearColor[0];
    att.clearValue.color.float32[1] = clearColor[1];
    att.clearValue.color.float32[2] = clearColor[2];
    att.clearValue.color.float32[3] = clearColor[3];
    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { { 0, 0 }, { width, height } };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &att;
    vkCmdBeginRendering(static_cast<VkCommandBuffer>(m_commandBuffer), &renderingInfo);
    m_isRenderingActive = true;
    m_currentRenderTarget = nullptr;
    m_activeDynamicRenderWidth = width;
    m_activeDynamicRenderHeight = height;
}

void CommandListVK::BeginRenderingWithTarget(RenderTargetVK* rtVK, bool doTransitions, const ::eyelib::rendering::RenderAttachmentOps* ops) {
    if (!rtVK || !m_isRecording || !m_commandBuffer || !m_device) return;
    const uint32_t colorCount = rtVK->GetColorAttachmentCount();
    VkImageView depthView = static_cast<VkImageView>(rtVK->GetDepthImageView());
    // When ops explicitly declares hasDepth=false, the pass intends to sample depth
    // as a shader resource — do NOT bind it as an attachment or transition it to DepthWrite.
    const bool useDepth = (ops ? ops->hasDepth : true) && (depthView != VK_NULL_HANDLE);
    if (colorCount == 0 && !useDepth) return;
    const uint32_t rtWidth = rtVK->GetWidth();
    const uint32_t rtHeight = rtVK->GetHeight();
    if (rtWidth == 0 || rtHeight == 0) return;

    if (doTransitions) {
        for (uint32_t i = 0; i < colorCount; ++i) {
            ITexture* tex = rtVK->GetColorTexture(i);
            if (tex && m_device->GetTextureTrackedState(tex) != ResourceState::RenderTarget)
                m_device->TransitionTextureToState(tex, ResourceState::RenderTarget);
        }
        if (useDepth) {
            ITexture* depthTex = rtVK->GetDepthTexture();
            if (depthTex && m_device->GetTextureTrackedState(depthTex) != ResourceState::DepthWrite)
                m_device->TransitionTextureToState(depthTex, ResourceState::DepthWrite);
        }
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
        colorAttachments[i].loadOp = ops ? ToVkLoadOp(ops->color.loadOp) : VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachments[i].storeOp = ops ? ToVkStoreOp(ops->color.storeOp) : VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachments[i].clearValue.color = {
            ops ? ops->color.clearValue.rgba[0] : 0.f,
            ops ? ops->color.clearValue.rgba[1] : 0.f,
            ops ? ops->color.clearValue.rgba[2] : 0.f,
            ops ? ops->color.clearValue.rgba[3] : 1.f
        };
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
    if (useDepth) {
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = depthView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = ops ? ToVkLoadOp(ops->depth.loadOp) : VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = ops ? ToVkStoreOp(ops->depth.storeOp) : VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = {
            ops ? ops->depth.clearValue.depth : 1.f,
            ops ? ops->depth.clearValue.stencil : 0u
        };
        renderingInfo.pDepthAttachment = &depthAttachment;
    }
    vkCmdBeginRendering(static_cast<VkCommandBuffer>(m_commandBuffer), &renderingInfo);
    m_isRenderingActive = true;
    m_currentRenderTarget = rtVK;
    m_activeDynamicRenderWidth = rtWidth;
    m_activeDynamicRenderHeight = rtHeight;
}

void CommandListVK::SetRenderTarget(IRenderTarget* rt) {
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
    EndRenderingIfActive();
    if (!rt || !m_isRecording || !m_commandBuffer) return;
    RenderTargetVK* rtVK = static_cast<RenderTargetVK*>(rt);
    BeginRenderingWithTarget(rtVK, true, nullptr);
}

void CommandListVK::SetRenderTarget(IRenderTarget* rt, const ::eyelib::rendering::RenderAttachmentOps& ops) {
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
    EndRenderingIfActive();
    if (!rt || !m_isRecording || !m_commandBuffer) return;
    RenderTargetVK* rtVK = static_cast<RenderTargetVK*>(rt);
    BeginRenderingWithTarget(rtVK, true, &ops);
}

void CommandListVK::SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) {
    (void)colorAttachmentIndex;
    SetRenderTarget(rt);
}

void CommandListVK::EndDynamicRendering() {
    EndRenderingIfActive();
}

void CommandListVK::ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) {
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
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
        att.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
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
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
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

void CommandListVK::ClearDefaultFramebuffer(const ClearValue& clear) { m_hasGraphicsWork = true; m_computeQueueEligible = false; (void)clear; }

void CommandListVK::PushDebugUtilsLabel(const char* name, const float* colorRgba) {
    if (!m_isRecording || !m_commandBuffer || !m_device || !name) return;
    static const float kDefaultRgba[4] = { 0.78f, 0.78f, 0.82f, 1.f };
    const float* c = colorRgba ? colorRgba : kDefaultRgba;
    CmdDebugUtilsBeginLabel(m_device, m_commandBuffer, name, c);
}

void CommandListVK::PopDebugUtilsLabel() {
    if (!m_isRecording || !m_commandBuffer || !m_device) return;
    CmdDebugUtilsEndLabel(m_device, m_commandBuffer);
}

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

void CommandListVK::SetDepthBias(float constantFactor, float slopeFactor) {
    if (!m_isRecording || !m_commandBuffer) return;
    // depthBiasClamp = 0.0f = no clamping (Vulkan spec). constantFactor/slopeFactor set per-cascade.
    vkCmdSetDepthBias(static_cast<VkCommandBuffer>(m_commandBuffer), constantFactor, 0.0f, slopeFactor);
}

void CommandListVK::BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) {
    m_requiresQueueOwnershipTransfers = true;
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
    NoteCrossQueueBuffer(bufVK->m_buffer);
    VkBufferMemoryBarrier2 barrier2 = {};
    barrier2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier2.srcStageMask = ToVkPipelineStage2(fromState);
    barrier2.srcAccessMask = ToVkAccessFlags2(fromState);
    barrier2.dstStageMask = ToVkPipelineStage2(toState);
    barrier2.dstAccessMask = ToVkAccessFlags2(toState);
    barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.buffer = vkBuf;
    barrier2.offset = 0;
    barrier2.size = VK_WHOLE_SIZE;
    VkDependencyInfo dep = {};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.bufferMemoryBarrierCount = 1;
    dep.pBufferMemoryBarriers = &barrier2;
    vkCmdPipelineBarrier2(static_cast<VkCommandBuffer>(m_commandBuffer), &dep);
}

void CommandListVK::BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) {
    m_requiresQueueOwnershipTransfers = true;
    if (!texture || !m_isRecording || !m_commandBuffer) return;
    if (m_isRenderingActive) {
        static int warnCount = 0;
        if (warnCount++ < 8)
            core::Log::Warn("Vulkan BarrierTexture called during active rendering; ending render scope explicitly. Move barriers outside render passes.");
    }
    EndRenderingIfActive();
    if (!m_device) return;
    TextureVK* texVK = static_cast<TextureVK*>(texture);
    BarrierTrackerVK* tracker = static_cast<BarrierTrackerVK*>(m_device->GetBarrierTracker());
    const ResourceState layoutBefore = BarrierTrackerVK::GetImageState(tracker, texVK->m_image);
    if (fromState != ResourceState::Undefined && layoutBefore != ResourceState::Undefined && fromState != layoutBefore) {
        static int mismatchWarn = 0;
        if (mismatchWarn < 8) {
            ++mismatchWarn;
            core::Log::WarnF(
                "CommandListVK::BarrierTexture: caller `fromState` (%u) != BarrierTracker layout (%u); emitted barrier uses tracker state.",
                static_cast<unsigned>(fromState), static_cast<unsigned>(layoutBefore));
        }
    }
    if (layoutBefore == ResourceState::Undefined) {
        m_computeQueueEligible = false;
        m_crossQueueOwnershipUnsafe = true;
    } else {
        const VkImageAspectFlags aspect = static_cast<VkImageAspectFlags>(ToVkImageAspect(texVK->GetFormat()));
        const uint32_t mipLevels = (std::max)(1u, texVK->m_mipLevels);
        const uint32_t layers = TextureVkArrayLayers(texVK);
        NoteCrossQueueImage(texVK->m_image, aspect, 0u, mipLevels, 0u, layers, layoutBefore);
    }
    m_device->TransitionTextureToState(texture, toState);
    m_device->FlushBarrierTracker(this);
}

void CommandListVK::MemoryBarrierStorage() {
    m_computeQueueEligible = false;
    m_requiresQueueOwnershipTransfers = true;
    m_crossQueueOwnershipUnsafe = true;
    if (!m_isRecording || !m_commandBuffer) return;
    if (m_isRenderingActive) {
        static int warnCount = 0;
        if (warnCount++ < 8)
            core::Log::Warn("Vulkan MemoryBarrierStorage called during active rendering; ending render scope explicitly. Move barriers outside render passes.");
    }
    EndRenderingIfActive();
    VkMemoryBarrier2 barrier2 = {};
    barrier2.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier2.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    barrier2.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier2.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    barrier2.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    VkDependencyInfo dep = {};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &barrier2;
    vkCmdPipelineBarrier2(static_cast<VkCommandBuffer>(m_commandBuffer), &dep);
}

void CommandListVK::BarrierBufferTransferReadToHostWrite(IBuffer* buffer) {
    m_computeQueueEligible = false;
    m_requiresQueueOwnershipTransfers = true;
    m_crossQueueOwnershipUnsafe = true;
    if (!buffer || !m_isRecording || !m_commandBuffer) return;
    if (m_isRenderingActive) {
        static int warnCount = 0;
        if (warnCount++ < 8)
            core::Log::Warn("Vulkan BarrierBufferTransferReadToHostWrite called during active rendering; ending render scope explicitly. Move barriers outside render passes.");
    }
    EndRenderingIfActive();
    BufferVK* bufVK = static_cast<BufferVK*>(buffer);
    VkBuffer vkBuf = static_cast<VkBuffer>(bufVK->m_buffer);
    if (vkBuf == VK_NULL_HANDLE) return;
    VkBufferMemoryBarrier2 barrier2 = {};
    barrier2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier2.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier2.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier2.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    barrier2.dstAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
    barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.buffer = vkBuf;
    barrier2.offset = 0;
    barrier2.size = VK_WHOLE_SIZE;
    VkDependencyInfo dep = {};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.bufferMemoryBarrierCount = 1;
    dep.pBufferMemoryBarriers = &barrier2;
    vkCmdPipelineBarrier2(static_cast<VkCommandBuffer>(m_commandBuffer), &dep);
}

void CommandListVK::BarrierHostVisibleBufferToTransferRead(IBuffer* buffer) {
    m_computeQueueEligible = false;
    m_requiresQueueOwnershipTransfers = true;
    m_crossQueueOwnershipUnsafe = true;
    if (!buffer || !m_isRecording || !m_commandBuffer) return;
    if (m_isRenderingActive) {
        static int warnCount = 0;
        if (warnCount++ < 8)
            core::Log::Warn("Vulkan BarrierHostVisibleBufferToTransferRead called during active rendering; ending render scope explicitly. Move barriers outside render passes.");
    }
    EndRenderingIfActive();
    BufferVK* bufVK = static_cast<BufferVK*>(buffer);
    VkBuffer vkBuf = static_cast<VkBuffer>(bufVK->m_buffer);
    if (vkBuf == VK_NULL_HANDLE) return;
    VkBufferMemoryBarrier2 barrier2 = {};
    barrier2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier2.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    barrier2.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
    barrier2.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier2.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.buffer = vkBuf;
    barrier2.offset = 0;
    barrier2.size = VK_WHOLE_SIZE;
    VkDependencyInfo dep = {};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.bufferMemoryBarrierCount = 1;
    dep.pBufferMemoryBarriers = &barrier2;
    vkCmdPipelineBarrier2(static_cast<VkCommandBuffer>(m_commandBuffer), &dep);
}
void CommandListVK::BindPixelPackBufferAsSSBO(void* pboHandle, uint32_t slot, size_t size) { (void)pboHandle; (void)slot; (void)size; }

void CommandListVK::PushGraphicsConstantsIfPossible() {
    if (!m_isRecording || !m_commandBuffer || !m_device || !m_currentPipeline) return;
    PipelineVK* pVK = static_cast<PipelineVK*>(m_currentPipeline);
    if (pVK->m_isCompute) return;
    VkPipelineLayout layout = static_cast<VkPipelineLayout>(pVK->m_nativePipelineLayout);
    if (layout == VK_NULL_HANDLE) {
        // PushOnly mesh pipelines store no native layout (zero sets); push must use the mesh push layout, not the default V|F-only layout.
        if (pVK->UsesMeshShaders() && m_device->GetMeshPushPipelineLayout())
            layout = static_cast<VkPipelineLayout>(m_device->GetMeshPushPipelineLayout());
        else
            layout = static_cast<VkPipelineLayout>(m_device->GetDefaultPipelineLayout());
    }
    if (layout == VK_NULL_HANDLE) return;
    VkShaderStageFlags pushStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
#if defined(VK_SHADER_STAGE_MESH_BIT_EXT)
    if (pVK->UsesMeshShaders())
        pushStages = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
#endif
    vkCmdPushConstants(static_cast<VkCommandBuffer>(m_commandBuffer), layout, pushStages, 0, 256, m_pushConstants);
}

void CommandListVK::BindPipeline(IPipeline* pipeline) {
    if (!pipeline || !m_isRecording || !m_commandBuffer || !m_device) return;
    PipelineVK* pVK = static_cast<PipelineVK*>(pipeline);
    if (pVK->m_isCompute) m_hasComputeWork = true;
    else { m_hasGraphicsWork = true; m_computeQueueEligible = false; }
    VkPipeline vkPipe = static_cast<VkPipeline>(pVK->m_pipeline);
    if (vkPipe == VK_NULL_HANDLE) {
        // Do not set m_currentPipeline: BindDescriptorSets and draws require a real VkPipeline bind.
        m_currentPipeline = nullptr;
        {
            static int s_bindPipelineNullWarned = 0;
            if (s_bindPipelineNullWarned < 4) {
                ++s_bindPipelineNullWarned;
                core::Log::WarnF("CommandListVK::BindPipeline: VkPipeline is VK_NULL_HANDLE (layoutKind=%s); subsequent draws will be skipped.",
                                 PipelineLayoutKindName(pVK->m_layoutKind));
            }
        }
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
    if (!m_isRenderingActive) {
        core::Log::Warn("DrawFullscreenQuad: not inside active dynamic rendering; draw skipped - output will not be written.");
        return;
    }
    const uint32_t rw = m_activeDynamicRenderWidth;
    const uint32_t rh = m_activeDynamicRenderHeight;
    if (rw == 0 || rh == 0) {
        core::Log::Warn("DrawFullscreenQuad: active render extent is zero; draw skipped.");
        return;
    }
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
    // Pipelines use dynamic viewport/scissor; must be set before draw (VUID-vkCmdDrawIndexed-None-07831/07832).
    SetViewport(0, 0, rw, rh);
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
    // Optional partial update: Forward/GBuffer bindless sets, or default compute (`GetComputePipelineLayout`: set1 bindless). Prefer
    // BindDescriptorSets(0, N, ...) with all resource-space sets when possible; this exists for incremental/tooling use after vkCmdBindPipeline.
    if (!m_isRecording || !m_commandBuffer || !m_device || !m_currentPipeline) return;
    PipelineVK* pVK = static_cast<PipelineVK*>(m_currentPipeline);
    const uint32_t setIndex = BindlessHeapSetIndexForCurrentPipeline(m_device, pVK);
    if (setIndex == UINT32_MAX) return;
    if (!pVK->m_isCompute) {
        if (pVK->m_requiredSetCount == 0 || pVK->m_requiredSetCount <= setIndex) return;
    }

    const VkPipelineLayout layout = ResolvePipelineLayoutForDescriptorBind(m_device, pVK);
    if (layout == VK_NULL_HANDLE) return;

#if OPENEYE_RHI_BINDLESS_PROFILE
    const std::chrono::steady_clock::time_point profileT0 = std::chrono::steady_clock::now();
#endif

    BindlessHeapVK* heapVK = heap ? static_cast<BindlessHeapVK*>(heap) : nullptr;
    const VkPipelineBindPoint bindPoint = pVK->m_isCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

#if defined(VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT) && defined(VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT)
    if (m_device->SupportsDescriptorBuffer() && heapVK && heapVK->m_usesDescriptorBuffer && heapVK->m_descriptorBuffer &&
        m_device->DescriptorBufferFnCmdBindBuffersEXT() && m_device->DescriptorBufferFnCmdSetOffsetsEXT()) {
        BufferVK* bufVK = static_cast<BufferVK*>(heapVK->m_descriptorBuffer);
        VkBuffer vkBuf = bufVK->m_buffer ? static_cast<VkBuffer>(bufVK->m_buffer) : VK_NULL_HANDLE;
        if (vkBuf != VK_NULL_HANDLE) {
            m_requiresQueueOwnershipTransfers = true;
            EndRenderingIfActive();
            VkBufferMemoryBarrier2 barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = vkBuf;
            barrier.offset = 0;
            barrier.size = heapVK->m_descriptorBufferSize > 0 ? static_cast<VkDeviceSize>(heapVK->m_descriptorBufferSize) : VK_WHOLE_SIZE;
            VkDependencyInfo dep = {};
            dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dep.bufferMemoryBarrierCount = 1;
            dep.pBufferMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(static_cast<VkCommandBuffer>(m_commandBuffer), &dep);

            VkDescriptorBufferBindingInfoEXT bindInfo = {};
            bindInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
            bindInfo.buffer = vkBuf;
            bindInfo.offset = 0;
            bindInfo.range = heapVK->m_descriptorBufferSize;
            bindInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

            auto pfnBind =
                reinterpret_cast<PFN_vkCmdBindDescriptorBuffersEXT>(m_device->DescriptorBufferFnCmdBindBuffersEXT());
            pfnBind(static_cast<VkCommandBuffer>(m_commandBuffer), 0, 1, &bindInfo);

            const uint32_t bufIdx = heapVK->m_descriptorBufferBindingIndex;
            const VkDeviceSize dbOffset = 0;
            auto pfnSet =
                reinterpret_cast<PFN_vkCmdSetDescriptorBufferOffsetsEXT>(m_device->DescriptorBufferFnCmdSetOffsetsEXT());
            pfnSet(static_cast<VkCommandBuffer>(m_commandBuffer), bindPoint, layout, setIndex, 1, &bufIdx, &dbOffset);
#if OPENEYE_RHI_BINDLESS_PROFILE
            const std::chrono::steady_clock::time_point profileT1 = std::chrono::steady_clock::now();
            LogBindlessHeapBindProfile(BindlessBindProfilePath::DescriptorBuffer,
                std::chrono::duration_cast<std::chrono::microseconds>(profileT1 - profileT0).count());
#endif
            return;
        }
    }
#endif

    // Null heap or no DB: classic partially-bound set (update-after-bind pool); valid with independent sets + DB layouts.
    VkDescriptorSet vkHeapSet = static_cast<VkDescriptorSet>(m_device->GetEmptyBindlessSet());
    if (heapVK && heapVK->m_descriptorSet != nullptr)
        vkHeapSet = static_cast<VkDescriptorSet>(heapVK->m_descriptorSet);
    vkCmdBindDescriptorSets(static_cast<VkCommandBuffer>(m_commandBuffer), bindPoint, layout, setIndex, 1, &vkHeapSet, 0, nullptr);
#if OPENEYE_RHI_BINDLESS_PROFILE
    {
        const std::chrono::steady_clock::time_point profileT1 = std::chrono::steady_clock::now();
        LogBindlessHeapBindProfile(BindlessBindProfilePath::ClassicSet,
            std::chrono::duration_cast<std::chrono::microseconds>(profileT1 - profileT0).count());
    }
#endif
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
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
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
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
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
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
    DrawIndexed(indexCount, firstIndex, vertexOffset);
}
void CommandListVK::DrawIndexedInstanced(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t instanceCount) {
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
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
void CommandListVK::DrawArraysIndirect(IBuffer* indirectBuffer, size_t offset) {
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
    if (!m_isRecording || !m_commandBuffer) return;
    if (!m_isRenderingActive) return;
    VkBuffer buf = VkBufferFromRhi(indirectBuffer);
    if (buf == VK_NULL_HANDLE) return;
    PushGraphicsConstantsIfPossible();
    vkCmdDrawIndirect(static_cast<VkCommandBuffer>(m_commandBuffer), buf, static_cast<VkDeviceSize>(offset), 1u,
                      sizeof(VkDrawIndirectCommand));
}
void CommandListVK::DrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset) {
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
    if (!m_isRecording || !m_commandBuffer) return;
    if (!m_isRenderingActive) return;
    VkBuffer buf = VkBufferFromRhi(indirectBuffer);
    if (buf == VK_NULL_HANDLE) return;
    PushGraphicsConstantsIfPossible();
    vkCmdDrawIndexedIndirect(static_cast<VkCommandBuffer>(m_commandBuffer), buf, static_cast<VkDeviceSize>(offset), 1u,
                             sizeof(VkDrawIndexedIndirectCommand));
}

void CommandListVK::DrawMeshTasks(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
#if defined(VK_SHADER_STAGE_MESH_BIT_EXT)
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
    if (!m_device || !m_device->SupportsMeshShader() || !m_device->GetCmdDrawMeshTasksEXT()) return;
    if (!m_isRecording || !m_commandBuffer) return;
    if (!m_isRenderingActive) return;
    PipelineVK* pVK = m_currentPipeline ? static_cast<PipelineVK*>(m_currentPipeline) : nullptr;
    if (!pVK || !pVK->UsesMeshShaders()) {
        static int s_once = 0;
        if (s_once < 4) {
            ++s_once;
            core::Log::Warn("DrawMeshTasks: no mesh graphics pipeline bound; draw skipped.");
        }
        return;
    }
    PushGraphicsConstantsIfPossible();
    auto* pfn = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(m_device->GetCmdDrawMeshTasksEXT());
    pfn(static_cast<VkCommandBuffer>(m_commandBuffer), groupCountX, groupCountY, groupCountZ);
#else
    (void)groupCountX;
    (void)groupCountY;
    (void)groupCountZ;
#endif
}

void CommandListVK::DrawMeshTasksIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
#if defined(VK_SHADER_STAGE_MESH_BIT_EXT)
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
    if (!m_device || !m_device->SupportsMeshShader() || !m_device->GetCmdDrawMeshTasksIndirectEXT()) return;
    if (!m_isRecording || !m_commandBuffer) return;
    if (!m_isRenderingActive || drawCount == 0) return;
    PipelineVK* pVK = m_currentPipeline ? static_cast<PipelineVK*>(m_currentPipeline) : nullptr;
    if (!pVK || !pVK->UsesMeshShaders()) return;
    VkBuffer buf = VkBufferFromRhi(indirectBuffer);
    if (buf == VK_NULL_HANDLE) return;
    const uint32_t s = stride ? stride : 12u; // VkDrawMeshTasksIndirectCommandEXT: 3 x uint32_t group counts
    PushGraphicsConstantsIfPossible();
    auto* pfn = reinterpret_cast<PFN_vkCmdDrawMeshTasksIndirectEXT>(m_device->GetCmdDrawMeshTasksIndirectEXT());
    pfn(static_cast<VkCommandBuffer>(m_commandBuffer), buf, static_cast<VkDeviceSize>(offset), drawCount, s);
#else
    (void)indirectBuffer;
    (void)offset;
    (void)drawCount;
    (void)stride;
#endif
}

void CommandListVK::SetFragmentShadingRate(FragmentShadingRate rate,
                                           FragmentShadingRateCombinerOp pipelinePrimitiveOp,
                                           FragmentShadingRateCombinerOp primitiveAttachmentOp) {
#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR)
    if (!m_isRecording || !m_commandBuffer || !m_device || !m_device->SupportsVariableRateShading() ||
        !m_device->GetCmdSetFragmentShadingRateKHR()) {
        return;
    }
    const VkExtent2D fragmentSize = ToVkFragmentShadingRateExtent(rate);
    VkFragmentShadingRateCombinerOpKHR combiners[2] = {
        ToVkFragmentShadingRateCombinerOp(pipelinePrimitiveOp),
        ToVkFragmentShadingRateCombinerOp(primitiveAttachmentOp),
    };
    auto* pfn = reinterpret_cast<PFN_vkCmdSetFragmentShadingRateKHR>(m_device->GetCmdSetFragmentShadingRateKHR());
    pfn(static_cast<VkCommandBuffer>(m_commandBuffer), &fragmentSize, combiners);
#else
    (void)rate;
    (void)pipelinePrimitiveOp;
    (void)primitiveAttachmentOp;
#endif
}
void CommandListVK::MultiDrawArraysIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
    if (!m_isRecording || !m_commandBuffer) return;
    if (!m_isRenderingActive || drawCount == 0) return;
    VkBuffer buf = VkBufferFromRhi(indirectBuffer);
    if (buf == VK_NULL_HANDLE) return;
    const uint32_t s = stride ? stride : static_cast<uint32_t>(sizeof(VkDrawIndirectCommand));
    PushGraphicsConstantsIfPossible();
    vkCmdDrawIndirect(static_cast<VkCommandBuffer>(m_commandBuffer), buf, static_cast<VkDeviceSize>(offset), drawCount, s);
}
void CommandListVK::MultiDrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
    if (!m_isRecording || !m_commandBuffer) return;
    if (!m_isRenderingActive || drawCount == 0) return;
    VkBuffer buf = VkBufferFromRhi(indirectBuffer);
    if (buf == VK_NULL_HANDLE) return;
    const uint32_t s = stride ? stride : static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand));
    PushGraphicsConstantsIfPossible();
    vkCmdDrawIndexedIndirect(static_cast<VkCommandBuffer>(m_commandBuffer), buf, static_cast<VkDeviceSize>(offset), drawCount, s);
}
void CommandListVK::MultiDrawArraysIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer,
                                                 size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
    if (!m_device || !m_isRecording || !m_commandBuffer) return;
    if (!m_device->GetCapabilities().supportsMultiDrawIndirectCount) {
        static int once = 0;
        if (once++ == 0)
            core::Log::Warn("MultiDrawArraysIndirectCount: drawIndirectCount not supported on this device.");
        return;
    }
    if (!m_isRenderingActive || maxDrawCount == 0) return;
    VkBuffer ind = VkBufferFromRhi(indirectBuffer);
    VkBuffer cnt = VkBufferFromRhi(countBuffer);
    if (ind == VK_NULL_HANDLE || cnt == VK_NULL_HANDLE) return;
    const uint32_t s = stride ? stride : static_cast<uint32_t>(sizeof(VkDrawIndirectCommand));
    PushGraphicsConstantsIfPossible();
    vkCmdDrawIndirectCount(static_cast<VkCommandBuffer>(m_commandBuffer), ind, static_cast<VkDeviceSize>(indirectOffset), cnt,
                           static_cast<VkDeviceSize>(countBufferOffset), maxDrawCount, s);
}
void CommandListVK::MultiDrawIndexedIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer,
                                                  size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    m_hasGraphicsWork = true;
    m_computeQueueEligible = false;
    if (!m_device || !m_isRecording || !m_commandBuffer) return;
    if (!m_device->GetCapabilities().supportsMultiDrawIndirectCount) {
        static int once = 0;
        if (once++ == 0)
            core::Log::Warn("MultiDrawIndexedIndirectCount: drawIndirectCount not supported on this device.");
        return;
    }
    if (!m_isRenderingActive || maxDrawCount == 0) return;
    VkBuffer ind = VkBufferFromRhi(indirectBuffer);
    VkBuffer cnt = VkBufferFromRhi(countBuffer);
    if (ind == VK_NULL_HANDLE || cnt == VK_NULL_HANDLE) return;
    const uint32_t s = stride ? stride : static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand));
    PushGraphicsConstantsIfPossible();
    vkCmdDrawIndexedIndirectCount(static_cast<VkCommandBuffer>(m_commandBuffer), ind, static_cast<VkDeviceSize>(indirectOffset),
                                  cnt, static_cast<VkDeviceSize>(countBufferOffset), maxDrawCount, s);
}
void CommandListVK::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    m_hasComputeWork = true;
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
void CommandListVK::DispatchIndirect(IBuffer* indirectBuffer, size_t offset) {
    m_hasComputeWork = true;
    if (!m_isRecording || !m_commandBuffer || !m_device) return;
    VkBuffer buf = VkBufferFromRhi(indirectBuffer);
    if (buf == VK_NULL_HANDLE) return;
    PipelineVK* pVK = m_currentPipeline ? static_cast<PipelineVK*>(m_currentPipeline) : nullptr;
    if (!pVK || !pVK->m_isCompute || pVK->m_pipeline == VK_NULL_HANDLE) return;
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
    vkCmdDispatchIndirect(static_cast<VkCommandBuffer>(m_commandBuffer), buf, static_cast<VkDeviceSize>(offset));
}

void CommandListVK::ResetQueryPool(IQueryPool* pool, uint32_t firstQuery, uint32_t queryCount) {
    if (!m_isRecording || !m_commandBuffer) return;
    if (!pool) {
        core::Log::Error("CommandListVK::ResetQueryPool: query pool is null.");
        return;
    }
    if (queryCount == 0) {
        core::Log::Error("CommandListVK::ResetQueryPool: queryCount must be > 0.");
        return;
    }
    QueryPoolVK* qvk = static_cast<QueryPoolVK*>(pool);
    VkQueryPool qp = static_cast<VkQueryPool>(qvk->m_queryPool);
    if (qp == VK_NULL_HANDLE) return;
    const uint64_t end = static_cast<uint64_t>(firstQuery) + static_cast<uint64_t>(queryCount);
    if (end > static_cast<uint64_t>(qvk->m_count)) {
        core::Log::ErrorF("CommandListVK::ResetQueryPool: range exceeds pool count (first=%u count=%u poolCount=%u).", firstQuery,
                          queryCount, qvk->m_count);
        return;
    }
    for (uint32_t i = 0; i < queryCount; ++i) {
        const uint64_t key = MakeQueryKey(pool, firstQuery + i);
        if (m_activeQueryKeys.find(key) != m_activeQueryKeys.end()) {
            core::Log::ErrorF("CommandListVK::ResetQueryPool: query index %u is active; end query before reset.", firstQuery + i);
            return;
        }
    }
    if (m_isRenderingActive) {
        if (::eyelib::rendering::backend::SupportsHostQueryReset(m_device)) {
            m_device->ResetQueryPoolOnHost(pool, firstQuery, queryCount);
            return;
        }
        core::Log::Error("CommandListVK::ResetQueryPool: vkCmdResetQueryPool is not valid inside dynamic rendering (enable host query reset or end the pass).");
        return;
    }
    vkCmdResetQueryPool(static_cast<VkCommandBuffer>(m_commandBuffer), qp, firstQuery, queryCount);
}

void CommandListVK::BeginQuery(IQueryPool* pool, uint32_t index) {
    if (!m_isRecording || !m_commandBuffer || !pool) return;
    QueryPoolVK* qvk = static_cast<QueryPoolVK*>(pool);
    VkQueryPool qp = static_cast<VkQueryPool>(qvk->m_queryPool);
    if (qp == VK_NULL_HANDLE) return;
    if (qvk->GetType() == QueryType::Timestamp) {
        if (index + 1u >= qvk->m_count) {
            core::Log::ErrorF("CommandListVK::BeginQuery: timestamp pair needs index+1 < pool count (index=%u count=%u).", index,
                              qvk->m_count);
            return;
        }
        if (::eyelib::rendering::backend::SupportsHostQueryReset(m_device))
            m_device->ResetQueryPoolOnHost(pool, index, 2u);
        else if (m_isRenderingActive) {
            core::Log::Error("CommandListVK::BeginQuery(Timestamp): vkCmdResetQueryPool is not valid inside dynamic rendering; "
                             "enable host query reset, or call BeginQuery after ending the pass.");
            return;
        } else {
            vkCmdResetQueryPool(static_cast<VkCommandBuffer>(m_commandBuffer), qp, index, 2u);
        }
        vkCmdWriteTimestamp(static_cast<VkCommandBuffer>(m_commandBuffer), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qp, index);
        m_activeQueryKeys.insert(MakeQueryKey(pool, index));
    } else if (qvk->GetType() == QueryType::Occlusion) {
        if (index >= qvk->m_count) {
            core::Log::ErrorF("CommandListVK::BeginQuery(Occlusion): index out of range (index=%u count=%u).", index, qvk->m_count);
            return;
        }
        const uint64_t key = MakeQueryKey(pool, index);
        if (!m_activeQueryKeys.insert(key).second) {
            core::Log::ErrorF("CommandListVK::BeginQuery(Occlusion): query index %u is already active.", index);
            return;
        }
        vkCmdBeginQuery(static_cast<VkCommandBuffer>(m_commandBuffer), qp, index, 0u);
    } else if (qvk->GetType() == QueryType::PipelineStatistics) {
        if (index >= qvk->m_count) {
            core::Log::ErrorF("CommandListVK::BeginQuery(PipelineStatistics): index out of range (index=%u count=%u).", index,
                              qvk->m_count);
            return;
        }
        const uint64_t key = MakeQueryKey(pool, index);
        if (!m_activeQueryKeys.insert(key).second) {
            core::Log::ErrorF("CommandListVK::BeginQuery(PipelineStatistics): query index %u is already active.", index);
            return;
        }
        vkCmdBeginQuery(static_cast<VkCommandBuffer>(m_commandBuffer), qp, index, 0u);
    } else {
        core::Log::Error("CommandListVK::BeginQuery: unsupported query type for Vulkan command list.");
    }
}

void CommandListVK::EndQuery(IQueryPool* pool, uint32_t index) {
    if (!m_isRecording || !m_commandBuffer || !pool) return;
    QueryPoolVK* qvk = static_cast<QueryPoolVK*>(pool);
    VkQueryPool qp = static_cast<VkQueryPool>(qvk->m_queryPool);
    if (qp == VK_NULL_HANDLE) return;
    if (qvk->GetType() == QueryType::Timestamp) {
        if (index + 1u >= qvk->m_count) {
            core::Log::ErrorF("CommandListVK::EndQuery: timestamp pair needs index+1 < pool count (index=%u count=%u).", index,
                              qvk->m_count);
            return;
        }
        const uint64_t key = MakeQueryKey(pool, index);
        if (m_activeQueryKeys.erase(key) == 0) {
            core::Log::ErrorF("CommandListVK::EndQuery(Timestamp): query index %u is not active.", index);
            return;
        }
        vkCmdWriteTimestamp(static_cast<VkCommandBuffer>(m_commandBuffer), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qp, index + 1u);
    } else if (qvk->GetType() == QueryType::Occlusion) {
        if (index >= qvk->m_count) {
            core::Log::ErrorF("CommandListVK::EndQuery(Occlusion): index out of range (index=%u count=%u).", index, qvk->m_count);
            return;
        }
        const uint64_t key = MakeQueryKey(pool, index);
        if (m_activeQueryKeys.erase(key) == 0) {
            core::Log::ErrorF("CommandListVK::EndQuery(Occlusion): query index %u is not active.", index);
            return;
        }
        vkCmdEndQuery(static_cast<VkCommandBuffer>(m_commandBuffer), qp, index);
    } else if (qvk->GetType() == QueryType::PipelineStatistics) {
        if (index >= qvk->m_count) {
            core::Log::ErrorF("CommandListVK::EndQuery(PipelineStatistics): index out of range (index=%u count=%u).", index,
                              qvk->m_count);
            return;
        }
        const uint64_t key = MakeQueryKey(pool, index);
        if (m_activeQueryKeys.erase(key) == 0) {
            core::Log::ErrorF("CommandListVK::EndQuery(PipelineStatistics): query index %u is not active.", index);
            return;
        }
        vkCmdEndQuery(static_cast<VkCommandBuffer>(m_commandBuffer), qp, index);
    } else {
        core::Log::Error("CommandListVK::EndQuery: unsupported query type for Vulkan command list.");
    }
}

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
    m_requiresQueueOwnershipTransfers = true;
    if (!m_isRecording || !m_commandBuffer || !src || !dst || size == 0) return;
    BufferVK* srcVK = static_cast<BufferVK*>(src);
    BufferVK* dstVK = static_cast<BufferVK*>(dst);
    if (!srcVK->m_buffer || !dstVK->m_buffer) return;
    NoteCrossQueueBuffer(srcVK->m_buffer);
    NoteCrossQueueBuffer(dstVK->m_buffer);
    VkBufferCopy region = {};
    region.srcOffset = srcOffset;
    region.dstOffset = dstOffset;
    region.size = size;
    vkCmdCopyBuffer(static_cast<VkCommandBuffer>(m_commandBuffer),
                    static_cast<VkBuffer>(srcVK->m_buffer),
                    static_cast<VkBuffer>(dstVK->m_buffer),
                    1, &region);
}

// Phase 11.4 — `VK_KHR_acceleration_structure` device build + barriers (ray-query slice; not RTPSO / traceRays).
void CommandListVK::BuildAccelerationStructures(uint32_t infoCount, const void* pBuildGeometryInfos, const void* const* ppBuildRangeInfos) {
    m_computeQueueEligible = false;
    m_requiresQueueOwnershipTransfers = true;
    m_crossQueueOwnershipUnsafe = true;
    if (!m_device || !m_isRecording || !m_commandBuffer || infoCount == 0 || !pBuildGeometryInfos || !ppBuildRangeInfos) return;
    if (!m_device->GetCapabilities().supportsAccelerationStructureBuild) return;
    if (m_isRenderingActive) {
        static int warnCount = 0;
        if (warnCount++ < 8)
            core::Log::Warn(
                "Vulkan BuildAccelerationStructures called during active rendering; ending render scope explicitly. Move AS build outside render passes.");
    }
    EndRenderingIfActive();
#if defined(VK_KHR_acceleration_structure)
    auto* pfnBuild = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(m_device->GetCmdBuildAccelerationStructuresKHR());
    if (pfnBuild) {
        pfnBuild(static_cast<VkCommandBuffer>(m_commandBuffer), infoCount,
                 reinterpret_cast<const VkAccelerationStructureBuildGeometryInfoKHR*>(pBuildGeometryInfos),
                 reinterpret_cast<const VkAccelerationStructureBuildRangeInfoKHR* const*>(ppBuildRangeInfos));
    }
#endif
}

void CommandListVK::MemoryBarrierAccelerationStructureAccess() {
    m_computeQueueEligible = false;
    m_requiresQueueOwnershipTransfers = true;
    m_crossQueueOwnershipUnsafe = true;
    if (!m_device || !m_isRecording || !m_commandBuffer) return;
    if (!m_device->GetCapabilities().supportsAccelerationStructureBuild) return;
    if (m_isRenderingActive) {
        static int warnCount = 0;
        if (warnCount++ < 8)
            core::Log::Warn(
                "Vulkan MemoryBarrierAccelerationStructureAccess called during active rendering; ending render scope explicitly. Move barriers outside "
                "render passes.");
    }
    EndRenderingIfActive();
#if defined(VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR) && defined(VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR)
    VkMemoryBarrier2 barrier2 = {};
    barrier2.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier2.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    barrier2.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier2.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    barrier2.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    VkDependencyInfo dep = {};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &barrier2;
    vkCmdPipelineBarrier2(static_cast<VkCommandBuffer>(m_commandBuffer), &dep);
#endif
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
