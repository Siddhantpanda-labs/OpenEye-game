#include "eyelib/render/rhi/Vulkan/DeviceVk.h"
#include "eyelib/render/rhi/Vulkan/CommandListVk.h"
#include "eyelib/render/rhi/Vulkan/BufferVk.h"
#include "eyelib/render/rhi/Vulkan/TextureVk.h"
#include "eyelib/render/rhi/Vulkan/RenderTargetVk.h"
#include "eyelib/render/rhi/Vulkan/ShaderVk.h"
#include "eyelib/render/rhi/Vulkan/PipelineVk.h"
#include "eyelib/render/rhi/Vulkan/QueryPoolVk.h"
#include "eyelib/render/rhi/Vulkan/VkEnums.h"
#include "eyelib/render/rhi/Vulkan/vma/vk_mem_alloc.h"
#include <algorithm>
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

namespace {

constexpr uint32_t kMaxColorAttachments = 8;

} // namespace

DeviceVk::DeviceVk(const VulkanDeviceCreateInfo& info)
    : m_instance(info.instance)
    , m_physicalDevice(info.physicalDevice)
    , m_device(info.device)
    , m_graphicsQueueFamilyIndex(info.graphicsQueueFamilyIndex)
    , m_graphicsQueue(info.graphicsQueue)
    , m_surface(info.surface)
    , m_presentQueueFamilyIndex(info.presentQueueFamilyIndex)
    , m_presentQueue(info.presentQueue)
    , m_minImageCount(info.minImageCount) {
    if (m_presentQueue == VK_NULL_HANDLE) m_presentQueue = m_graphicsQueue;
    {
        VkAttachmentDescription colorAtt = {};
        colorAtt.format = VK_FORMAT_R8G8B8A8_UNORM;
        colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAtt.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAtt.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        VkRenderPassCreateInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = 1;
        rpInfo.pAttachments = &colorAtt;
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpass;
        vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_defaultRenderPass);
    }
    if (m_surface != VK_NULL_HANDLE) CreateSwapchain(0, 0);
    CreateAllocator();
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_graphicsCommandPool) != VK_SUCCESS)
        m_graphicsCommandPool = VK_NULL_HANDLE;
    m_immediateCommandList = std::make_unique<CommandListVk>(this);
}

DeviceVk::~DeviceVk() {
    if (m_immediateCommandList) m_immediateCommandList->End();
    m_immediateCommandList.reset();
    vkDestroyCommandPool(m_device, m_graphicsCommandPool, nullptr);
    m_graphicsCommandPool = VK_NULL_HANDLE;
    if (m_defaultRenderPass != VK_NULL_HANDLE) { vkDestroyRenderPass(m_device, m_defaultRenderPass, nullptr); m_defaultRenderPass = VK_NULL_HANDLE; }
    DestroySwapchain();
    DestroyAllocator();
}

void DeviceVk::CreateAllocator() {
    VmaAllocatorCreateInfo allocInfo = {};
    allocInfo.physicalDevice = m_physicalDevice;
    allocInfo.device = m_device;
    allocInfo.instance = m_instance;
    allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    if (vmaCreateAllocator(&allocInfo, &m_allocator) != VK_SUCCESS)
        m_allocator = nullptr;
}

void DeviceVk::DestroyAllocator() {
    if (m_allocator) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = nullptr;
    }
}

void DeviceVk::CreateSwapchain(uint32_t width, uint32_t height) {
    if (m_surface == VK_NULL_HANDLE) return;
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount) vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());
    m_swapchainFormat = (formatCount && formats[0].format != VK_FORMAT_UNDEFINED) ? formats[0].format : VK_FORMAT_B8G8R8A8_UNORM;
    if (width == 0) width = caps.currentExtent.width;
    if (height == 0) height = caps.currentExtent.height;
    if (width == 0 || height == 0) width = height = 1024;
    m_swapchainExtent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
    m_swapchainExtent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    m_swapchainImageCount = std::clamp(m_minImageCount, caps.minImageCount, caps.maxImageCount ? caps.maxImageCount : 3);
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = m_swapchainImageCount;
    createInfo.imageFormat = m_swapchainFormat;
    createInfo.imageColorSpace = formatCount ? formats[0].colorSpace : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = m_swapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
        m_swapchain = VK_NULL_HANDLE;
        return;
    }
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageCount, nullptr);
    m_swapchainImages.resize(m_swapchainImageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageCount, m_swapchainImages.data());
    m_swapchainImageViews.resize(m_swapchainImageCount);
    for (uint32_t i = 0; i < m_swapchainImageCount; ++i) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_swapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS)
            m_swapchainImageViews[i] = VK_NULL_HANDLE;
    }
    VkAttachmentDescription colorAtt = {};
    colorAtt.format = m_swapchainFormat;
    colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAtt.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAtt;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    if (vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_swapchainRenderPass) != VK_SUCCESS)
        m_swapchainRenderPass = VK_NULL_HANDLE;
    m_swapchainFramebuffers.resize(m_swapchainImageCount);
    for (uint32_t i = 0; i < m_swapchainImageCount; ++i) {
        if (!m_swapchainImageViews[i]) continue;
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_swapchainRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &m_swapchainImageViews[i];
        fbInfo.width = m_swapchainExtent.width;
        fbInfo.height = m_swapchainExtent.height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_swapchainFramebuffers[i]) != VK_SUCCESS)
            m_swapchainFramebuffers[i] = VK_NULL_HANDLE;
    }
}

void DeviceVk::DestroySwapchain() {
    for (auto fb : m_swapchainFramebuffers) { if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(m_device, fb, nullptr); }
    m_swapchainFramebuffers.clear();
    if (m_swapchainRenderPass != VK_NULL_HANDLE) { vkDestroyRenderPass(m_device, m_swapchainRenderPass, nullptr); m_swapchainRenderPass = VK_NULL_HANDLE; }
    for (auto v : m_swapchainImageViews) { if (v != VK_NULL_HANDLE) vkDestroyImageView(m_device, v, nullptr); }
    m_swapchainImageViews.clear();
    m_swapchainImages.clear();
    if (m_swapchain != VK_NULL_HANDLE) { vkDestroySwapchainKHR(m_device, m_swapchain, nullptr); m_swapchain = VK_NULL_HANDLE; }
}

void DeviceVk::ResizeSwapchain(uint32_t width, uint32_t height) {
    if (m_surface == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(m_device);
    DestroySwapchain();
    CreateSwapchain(width, height);
}

ICommandList* DeviceVk::GetImmediateCommandList() {
    return m_immediateCommandList.get();
}

void DeviceVk::SubmitCommandList(ICommandList* list) {
    (void)list;
    if (m_immediateCommandList.get() == list)
        m_immediateCommandList->Submit(m_graphicsQueue);
}

IBuffer* DeviceVk::CreateBuffer(const BufferDesc& desc, const void* initialData) {
    if (!m_allocator || desc.sizeInBytes == 0) return nullptr;
    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = desc.sizeInBytes;
    bufInfo.usage = vk::VkEnums::BufferUsageFlags(desc.usage);
    if (initialData) bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = (desc.memoryHint == BufferMemoryHint::HostVisible) ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocInfo.flags = (desc.memoryHint == BufferMemoryHint::HostVisible) ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0;
    auto* buf = new BufferVk();
    buf->m_device = this;
    buf->m_size = desc.sizeInBytes;
    buf->m_hint = desc.memoryHint;
    VkResult r = vmaCreateBuffer(m_allocator, &bufInfo, &allocInfo, &buf->m_buffer, &buf->m_allocation, nullptr);
    if (r != VK_SUCCESS) { delete buf; return nullptr; }
    if (initialData) buf->Update(initialData, 0, desc.sizeInBytes);
    return buf;
}

void DeviceVk::DestroyBuffer(IBuffer* buffer) {
    auto* b = static_cast<BufferVk*>(buffer);
    if (!b) return;
    if (m_allocator && b->m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, b->m_buffer, b->m_allocation);
        b->m_buffer = VK_NULL_HANDLE;
        b->m_allocation = nullptr;
    }
    delete b;
}

ITexture* DeviceVk::CreateTexture(const TextureDesc& desc, const void* initialData) {
    if (!m_allocator) return nullptr;
    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = vk::VkEnums::ImageType(desc.type);
    imgInfo.format = vk::VkEnums::Format(desc.format);
    imgInfo.extent.width = desc.width;
    imgInfo.extent.height = desc.height;
    imgInfo.extent.depth = (desc.type == TextureType::Texture3D) ? desc.depthOrArrayLayers : 1;
    imgInfo.mipLevels = desc.mipLevels;
    imgInfo.arrayLayers = (desc.type == TextureType::Texture2DArray || desc.type == TextureType::TextureCube) ? desc.depthOrArrayLayers : 1;
    imgInfo.samples = desc.samples > 1 ? static_cast<VkSampleCountFlagBits>(desc.samples) : VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = vk::VkEnums::ImageUsage(desc.usage);
    if (desc.format == RHITextureFormat::Depth24 || desc.format == RHITextureFormat::Depth24Stencil8)
        imgInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (desc.type == TextureType::TextureCube) imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    auto* tex = new TextureVk();
    tex->m_device = this;
    tex->m_width = desc.width;
    tex->m_height = desc.height;
    tex->m_depthOrArray = desc.depthOrArrayLayers;
    tex->m_type = desc.type;
    tex->m_format = desc.format;
    VkResult r = vmaCreateImage(m_allocator, &imgInfo, &allocInfo, &tex->m_image, &tex->m_allocation, nullptr);
    if (r != VK_SUCCESS) { delete tex; return nullptr; }
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tex->m_image;
    viewInfo.viewType = (desc.type == TextureType::TextureCube) ? VK_IMAGE_VIEW_TYPE_CUBE : (desc.type == TextureType::Texture2DArray) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : (desc.type == TextureType::Texture3D) ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange.aspectMask = (desc.format == RHITextureFormat::Depth24 || desc.format == RHITextureFormat::Depth24Stencil8) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    if (desc.format == RHITextureFormat::Depth24Stencil8) viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = desc.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = (desc.type == TextureType::TextureCube) ? 6 : (desc.type == TextureType::Texture2DArray) ? desc.depthOrArrayLayers : 1;
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &tex->m_view) != VK_SUCCESS) { DestroyTexture(tex); return nullptr; }
    if (initialData && desc.width > 0 && desc.height > 0) {
        size_t copySize = desc.width * desc.height * 4;
        if (desc.format == RHITextureFormat::RGBA16F || desc.format == RHITextureFormat::RGB16F) copySize = desc.width * desc.height * 8;
        if (desc.type == TextureType::TextureCube) copySize *= 6;
        if (desc.depthOrArrayLayers > 1 && desc.type != TextureType::Texture2DArray) copySize *= desc.depthOrArrayLayers;
        BufferDesc bd;
        bd.sizeInBytes = copySize;
        bd.usage = BufferUsage::TransferSrc;
        bd.memoryHint = BufferMemoryHint::HostVisible;
        IBuffer* staging = CreateBuffer(bd, initialData);
        if (staging) {
            m_immediateCommandList->Begin();
            m_immediateCommandList->BarrierTexture(tex, ResourceState::Undefined, ResourceState::TransferDst);
            static_cast<CommandListVk*>(m_immediateCommandList.get())->CopyBufferToImage(staging, 0, tex, 0, 0, 0);
            m_immediateCommandList->BarrierTexture(tex, ResourceState::TransferDst, ResourceState::ShaderResource);
            m_immediateCommandList->End();
            SubmitCommandList(m_immediateCommandList.get());
            vkQueueWaitIdle(m_graphicsQueue);
            DestroyBuffer(staging);
        }
    }
    return tex;
}

void DeviceVk::DestroyTexture(ITexture* texture) {
    auto* t = static_cast<TextureVk*>(texture);
    if (!t) return;
    if (t->m_view != VK_NULL_HANDLE) { vkDestroyImageView(m_device, t->m_view, nullptr); t->m_view = VK_NULL_HANDLE; }
    if (m_allocator && t->m_owned && t->m_image != VK_NULL_HANDLE) {
        vmaDestroyImage(m_allocator, t->m_image, t->m_allocation);
        t->m_image = VK_NULL_HANDLE;
        t->m_allocation = nullptr;
    }
    delete t;
}

ITexture* DeviceVk::CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) {
    auto* tex = new TextureVk();
    tex->m_device = this;
    tex->m_image = reinterpret_cast<VkImage>(nativeTextureId);
    tex->m_width = width;
    tex->m_height = height;
    tex->m_format = format;
    tex->m_owned = false;
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tex->m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vk::VkEnums::Format(format);
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &tex->m_view) != VK_SUCCESS) { delete tex; return nullptr; }
    return tex;
}

IRenderTarget* DeviceVk::CreateRenderTarget(uint32_t width, uint32_t height,
                                            const std::vector<RHITextureFormat>& colorFormats,
                                            RHITextureFormat depthFormat,
                                            uint32_t samples) {
    return RenderTargetVk::Create(this, width, height, colorFormats, depthFormat, samples, nullptr);
}

IRenderTarget* DeviceVk::CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) {
    return RenderTargetVk::CreateFromTexture(this, static_cast<TextureVk*>(colorTexture), face, mipLevel);
}

void DeviceVk::DestroyRenderTarget(IRenderTarget* rt) {
    RenderTargetVk::Destroy(this, static_cast<RenderTargetVk*>(rt));
}

IShader* DeviceVk::CreateShader(ShaderStage stage, const char* source, size_t sourceLen) {
    return ShaderVk::Create(this, stage, source, sourceLen);
}

void DeviceVk::DestroyShader(IShader* shader) {
    ShaderVk::Destroy(this, static_cast<ShaderVk*>(shader));
}

IPipeline* DeviceVk::CreateGraphicsPipeline(const PipelineDesc& desc) {
    return PipelineVk::CreateGraphics(this, desc);
}

IPipeline* DeviceVk::CreateComputePipeline(void* computeShader) {
    return PipelineVk::CreateCompute(this, static_cast<ShaderVk*>(computeShader));
}

void DeviceVk::DestroyPipeline(IPipeline* pipeline) {
    PipelineVk::Destroy(this, static_cast<PipelineVk*>(pipeline));
}

IQueryPool* DeviceVk::CreateQueryPool(QueryType type, uint32_t count) {
    return QueryPoolVk::Create(this, type, count);
}

void DeviceVk::DestroyQueryPool(IQueryPool* pool) {
    QueryPoolVk::Destroy(this, static_cast<QueryPoolVk*>(pool));
}

int DeviceVk::GetMaxSamples(int requestedSamples) {
    if (requestedSamples <= 0) return 0;
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;
    int maxSamples = 1;
    if (counts & VK_SAMPLE_COUNT_2_BIT) maxSamples = 2;
    if (counts & VK_SAMPLE_COUNT_4_BIT) maxSamples = 4;
    if (counts & VK_SAMPLE_COUNT_8_BIT) maxSamples = 8;
    if (counts & VK_SAMPLE_COUNT_16_BIT) maxSamples = 16;
    return std::min(requestedSamples, maxSamples);
}

void DeviceVk::Present() {
    if (m_swapchain == VK_NULL_HANDLE) return;
    static_cast<CommandListVk*>(m_immediateCommandList.get())->Present(m_swapchain, m_presentQueue, m_swapchainImageCount);
}

void DeviceVk::BlitToDefaultFramebuffer(IRenderTarget* src) {
    if (!src) return;
    auto* r = static_cast<RenderTargetVk*>(src);
    BlitToDefaultFramebuffer(r ? r->GetNativeFramebuffer() : nullptr, src->GetWidth(), src->GetHeight());
}

void DeviceVk::BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) {
    (void)nativeFbo;
    (void)width;
    (void)height;
    if (m_swapchain == VK_NULL_HANDLE) return;
    static_cast<CommandListVk*>(m_immediateCommandList.get())->BlitToSwapchain(this, width, height);
}

void DeviceVk::Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) {
    if (!src || !dst) return;
    static_cast<CommandListVk*>(m_immediateCommandList.get())->Blit(static_cast<RenderTargetVk*>(src), static_cast<RenderTargetVk*>(dst), colorOnly);
}

void DeviceVk::BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) {
    if (!dst) return;
    static_cast<CommandListVk*>(m_immediateCommandList.get())->BlitFromNative(nativeFbo, static_cast<RenderTargetVk*>(dst), colorOnly);
}

void DeviceVk::BlitDepthOnlyFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst) {
    if (!dst) return;
    static_cast<CommandListVk*>(m_immediateCommandList.get())->BlitDepthOnlyFromNative(nativeFbo, static_cast<RenderTargetVk*>(dst));
}

} // namespace rhi
} // namespace render
} // namespace eyelib
