#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/Vulkan/VulkanDeviceCreateInfo.h"
#include <vulkan/vulkan_core.h>
#include "eyelib/render/rhi/Vulkan/vma/vk_mem_alloc.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct CommandListVk;

struct DeviceVk : IDevice {
    explicit DeviceVk(const VulkanDeviceCreateInfo& info);
    ~DeviceVk() override;

    ICommandList* GetImmediateCommandList() override;
    void SubmitCommandList(ICommandList* list) override;

    IBuffer* CreateBuffer(const BufferDesc& desc, const void* initialData) override;
    void DestroyBuffer(IBuffer* buffer) override;

    ITexture* CreateTexture(const TextureDesc& desc, const void* initialData) override;
    void DestroyTexture(ITexture* texture) override;

    IShader* CreateShader(ShaderStage stage, const char* source, size_t sourceLen) override;
    void DestroyShader(IShader* shader) override;

    IPipeline* CreateGraphicsPipeline(const PipelineDesc& desc) override;
    IPipeline* CreateComputePipeline(void* computeShader) override;
    void DestroyPipeline(IPipeline* pipeline) override;

    IRenderTarget* CreateRenderTarget(uint32_t width, uint32_t height,
                                      const std::vector<RHITextureFormat>& colorFormats,
                                      RHITextureFormat depthFormat,
                                      uint32_t samples) override;
    IRenderTarget* CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) override;
    void DestroyRenderTarget(IRenderTarget* rt) override;

    IQueryPool* CreateQueryPool(QueryType type, uint32_t count) override;
    void DestroyQueryPool(IQueryPool* pool) override;

    int GetMaxSamples(int requestedSamples) override;
    ITexture* CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) override;

    void Present() override;
    void BlitToDefaultFramebuffer(IRenderTarget* src) override;
    void BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) override;
    void Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) override;
    void BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) override;
    void BlitDepthOnlyFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst) override;

    /** For VMA and internal use. */
    VmaAllocator GetAllocator() const { return m_allocator; }
    VkDevice GetDevice() const { return m_device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    VkCommandPool GetGraphicsCommandPool() const { return m_graphicsCommandPool; }
    uint32_t GetGraphicsQueueFamilyIndex() const { return m_graphicsQueueFamilyIndex; }

    /** Call when window is resized to recreate swapchain. */
    void ResizeSwapchain(uint32_t width, uint32_t height);

    /** Default render pass (1 color RGBA8) for pipeline creation. Compatible with same-format framebuffers. */
    VkRenderPass GetDefaultRenderPass() const { return m_defaultRenderPass; }

private:
    void CreateAllocator();
    void DestroyAllocator();
    void CreateSwapchain(uint32_t width, uint32_t height);
    void DestroySwapchain();

    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    uint32_t m_presentQueueFamilyIndex = 0;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    VmaAllocator m_allocator = nullptr;
    VkCommandPool m_graphicsCommandPool = VK_NULL_HANDLE;

    VkRenderPass m_defaultRenderPass = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<VkFramebuffer> m_swapchainFramebuffers;
    VkRenderPass m_swapchainRenderPass = VK_NULL_HANDLE;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent = {0, 0};
    uint32_t m_swapchainImageCount = 0;
    uint32_t m_minImageCount = 2;

    std::unique_ptr<CommandListVk> m_immediateCommandList;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
