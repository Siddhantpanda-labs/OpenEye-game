#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/ShaderVK.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/PipelineVK.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/PipelineLayoutVK.h"
#include "eyelib/rendering/rhi/Vulkan/descriptors/DescriptorSetVK.h"
#include "eyelib/rendering/rhi/Vulkan/barriers/BarrierTrackerVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/RenderTargetVK.h"
#include "eyelib/rendering/rhi/Vulkan/sync/QueryPoolVK.h"
#include "eyelib/rendering/rhi/Vulkan/sync/FenceVK.h"
#include "eyelib/rendering/rhi/Vulkan/descriptors/BindlessHeapVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/rendering/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/VulkanPipelineHash.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/VulkanBindingUtils.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/VulkanResourceSpaces.h"
#include "eyelib/rendering/rhi/ResourceSetLayoutDesc.h"
#include "eyelib/rendering/rhi/RHITypes.h"
#include "eyelib/rendering/shaders/ShaderPathResolve.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/core/Log.h"
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <set>
#include <string>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace {
// Per-frame transient allocator/cache pools (reset each BeginFrame for the active ring slot).
const VkDescriptorPoolSize kDescriptorPoolSizes[] = {
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
};
const uint32_t kDescriptorPoolSizeCount = static_cast<uint32_t>(sizeof(kDescriptorPoolSizes) / sizeof(kDescriptorPoolSizes[0]));
const uint32_t kDescriptorMaxSetsPerPool = 2048;
// Long-lived global pool for CreateDescriptorSet (PassResourceBuilder dynamic draw sets, etc.); must scale with visible draw counts × frames in flight.
const uint32_t kMainDescriptorPoolMaxSets = 16384;
const VkDescriptorPoolSize kMainDescriptorPoolSizes[] = {
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 65536 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16384 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16384 },
};
const uint32_t kMainDescriptorPoolSizeCount = static_cast<uint32_t>(sizeof(kMainDescriptorPoolSizes) / sizeof(kMainDescriptorPoolSizes[0]));
constexpr uint32_t kCorePipelineStatisticsMask = 0x7FFu;

std::string GetPipelineCachePathForPhysicalDevice(VkPhysicalDevice phys) {
    if (phys == VK_NULL_HANDLE) return {};
    VkPhysicalDeviceProperties props = {};
    vkGetPhysicalDeviceProperties(phys, &props);
    std::string exePath = eyelib::platform::filesystem::GetExecutablePath();
    std::string cacheDir = eyelib::platform::filesystem::GetDirectory(exePath);
    if (cacheDir.empty()) return {};
    char name[96];
    snprintf(name, sizeof(name), "vulkan_pipeline_cache_%u_%u.bin", props.vendorID, props.deviceID);
    return cacheDir + "/" + name;
}
}

namespace eyelib {
namespace rendering {
namespace rhi {

DeviceVK::DeviceVK() : m_caps{} {
    m_immediateCommandList = std::make_unique<CommandListVK>(this);
}

DeviceVK::~DeviceVK() {
    VkDevice device = static_cast<VkDevice>(m_device);
    // Ensure all submitted work has completed before destroying any resources (fixes validation on exit).
    if (device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device);
    // Flush deferred deletions so pipelines/buffers/images enqueued by pipeline shutdown are destroyed while GPU is idle.
    if (device != VK_NULL_HANDLE) {
        std::set<void*> alreadyDestroyedPipelines;
        for (uint32_t i = 0; i < kMaxFrames; ++i)
            m_deletionQueue.Tick(m_device, GetAllocator(), i, &alreadyDestroyedPipelines);
        RetirePendingTextureWrappers(true);
        for (IDescriptorSet* set : m_pendingDescriptorSetsToFree)
            DestroyDescriptorSetVK(this, set);
        m_pendingDescriptorSetsToFree.clear();
        // Destroy any VkPipeline still tracked in the graphics cache (handles not yet released via refcount).
        for (auto& kv : m_graphicsPipelineCache) {
            void* pl = kv.second.vkPipeline;
            if (pl && !alreadyDestroyedPipelines.count(pl)) {
                vkDestroyPipeline(device, static_cast<VkPipeline>(pl), nullptr);
                alreadyDestroyedPipelines.insert(pl);
            }
        }
        m_graphicsPipelineCache.clear();
    }

    if (m_fullscreenQuadVBO) { DestroyBuffer(m_fullscreenQuadVBO); m_fullscreenQuadVBO = nullptr; }
    if (m_fullscreenQuadIBO) { DestroyBuffer(m_fullscreenQuadIBO); m_fullscreenQuadIBO = nullptr; }
    DestroyPresentResolveResources();
    if (m_fallbackShaderResourceTexture2D) {
        DestroyTexture(m_fallbackShaderResourceTexture2D);
        m_fallbackShaderResourceTexture2D = nullptr;
    }
    if (m_fallbackShaderResourceTextureCube) {
        DestroyTexture(m_fallbackShaderResourceTextureCube);
        m_fallbackShaderResourceTextureCube = nullptr;
    }
    if (m_sharedBindlessHeap) {
        delete static_cast<BindlessHeapVK*>(m_sharedBindlessHeap);
        m_sharedBindlessHeap = nullptr;
    }
    m_sharedBindlessTextureHandles.clear();
    if (m_sharedShadowMapArrayDummyRt) {
        DestroyRenderTarget(m_sharedShadowMapArrayDummyRt);
        m_sharedShadowMapArrayDummyRt = nullptr;
        m_sharedShadowMapArrayDummyBarrierDone = false;
    }
    if (device != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < kMaxFrames; ++i)
            m_deletionQueue.Tick(m_device, GetAllocator(), i);
    }
    for (auto& bucket : m_frameSliceCommandListsByFrame)
        bucket.clear();
    m_transientRecordingStack.clear();
    m_immediateCommandList.reset();
    if (device != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < kMaxFrames; ++i)
            FrameContextVK::Destroy(&m_frameContexts[i], device);
    }
    VkSampler nearestSampler = static_cast<VkSampler>(m_nearestSampler);
    if (nearestSampler != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroySampler(device, nearestSampler, nullptr);
        m_nearestSampler = nullptr;
    }
    VkSampler defaultSampler = static_cast<VkSampler>(m_defaultSampler);
    if (defaultSampler != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroySampler(device, defaultSampler, nullptr);
        m_defaultSampler = nullptr;
    }
    DescriptorCacheVK::Destroy(&m_descriptorCache, device);
    DescriptorAllocatorVK::Destroy(&m_descriptorAllocator, device);
    VkDescriptorPool descriptorPool = static_cast<VkDescriptorPool>(m_descriptorPool);
    if (descriptorPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_descriptorPool = nullptr;
    }
    // Graphics pipelines were destroyed above (first destructor block) or cache is empty.
    m_graphicsPipelineCache.clear();
    VkPipelineCache pipelineCache = static_cast<VkPipelineCache>(m_pipelineCache);
    if (pipelineCache != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        size_t cacheSize = 0;
        if (vkGetPipelineCacheData(device, pipelineCache, &cacheSize, nullptr) == VK_SUCCESS && cacheSize > 0) {
            std::vector<uint8_t> cacheData(cacheSize);
            if (vkGetPipelineCacheData(device, pipelineCache, &cacheSize, cacheData.data()) == VK_SUCCESS) {
                VkPhysicalDevice phys = static_cast<VkPhysicalDevice>(m_physicalDeviceVK.GetHandle());
                std::string cachePath = GetPipelineCachePathForPhysicalDevice(phys);
                if (!cachePath.empty())
                    eyelib::platform::filesystem::WriteFile(cachePath, cacheData.data(), cacheData.size());
            }
        }
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
    }
    // Resource set layout wrappers (single-set IPipelineLayout per type; destroy before set layouts)
    for (size_t i = 0; i < 10; ++i) {
        if (m_resourceSetLayouts[i]) {
            DestroyPipelineLayout(m_resourceSetLayouts[i]);
            m_resourceSetLayouts[i] = nullptr;
        }
    }
    // Resource-space pipeline layouts (destroy before set layouts)
    VkPipelineLayout rscPl = static_cast<VkPipelineLayout>(m_forwardPipelineLayout);
    if (rscPl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, rscPl, nullptr); m_forwardPipelineLayout = nullptr; }
    rscPl = static_cast<VkPipelineLayout>(m_postPipelineLayout);
    if (rscPl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, rscPl, nullptr); m_postPipelineLayout = nullptr; }
    rscPl = static_cast<VkPipelineLayout>(m_shadowPipelineLayout);
    if (rscPl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, rscPl, nullptr); m_shadowPipelineLayout = nullptr; }
    rscPl = static_cast<VkPipelineLayout>(m_gbufferPipelineLayout);
    if (rscPl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, rscPl, nullptr); m_gbufferPipelineLayout = nullptr; }
    rscPl = static_cast<VkPipelineLayout>(m_deferredLightingPipelineLayout);
    if (rscPl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, rscPl, nullptr); m_deferredLightingPipelineLayout = nullptr; }
    rscPl = static_cast<VkPipelineLayout>(m_computeLightCullPipelineLayout);
    if (rscPl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, rscPl, nullptr); m_computeLightCullPipelineLayout = nullptr; }
    rscPl = static_cast<VkPipelineLayout>(m_computeGpuCullPipelineLayout);
    if (rscPl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, rscPl, nullptr); m_computeGpuCullPipelineLayout = nullptr; }
    rscPl = static_cast<VkPipelineLayout>(m_iblPipelineLayout);
    if (rscPl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, rscPl, nullptr); m_iblPipelineLayout = nullptr; }
    rscPl = static_cast<VkPipelineLayout>(m_outlinePostPipelineLayout);
    if (rscPl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, rscPl, nullptr); m_outlinePostPipelineLayout = nullptr; }
    // Resource-space set layouts
    VkDescriptorSetLayout rscLayout = static_cast<VkDescriptorSetLayout>(m_frameSetLayout);
    if (rscLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, rscLayout, nullptr); m_frameSetLayout = nullptr; }
    rscLayout = static_cast<VkDescriptorSetLayout>(m_forwardPassSetLayout);
    if (rscLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, rscLayout, nullptr); m_forwardPassSetLayout = nullptr; }
    rscLayout = static_cast<VkDescriptorSetLayout>(m_postPassSetLayout);
    if (rscLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, rscLayout, nullptr); m_postPassSetLayout = nullptr; }
    rscLayout = static_cast<VkDescriptorSetLayout>(m_shadowPassSetLayout);
    if (rscLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, rscLayout, nullptr); m_shadowPassSetLayout = nullptr; }
    rscLayout = static_cast<VkDescriptorSetLayout>(m_lightCullPassSetLayout);
    if (rscLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, rscLayout, nullptr); m_lightCullPassSetLayout = nullptr; }
    rscLayout = static_cast<VkDescriptorSetLayout>(m_deferredLightingPassSetLayout);
    if (rscLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, rscLayout, nullptr); m_deferredLightingPassSetLayout = nullptr; }
    rscLayout = static_cast<VkDescriptorSetLayout>(m_materialSetLayout);
    if (rscLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, rscLayout, nullptr); m_materialSetLayout = nullptr; }
    rscLayout = static_cast<VkDescriptorSetLayout>(m_sceneBuffersSetLayout);
    if (rscLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, rscLayout, nullptr); m_sceneBuffersSetLayout = nullptr; }
    rscLayout = static_cast<VkDescriptorSetLayout>(m_iblPassSetLayout);
    if (rscLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, rscLayout, nullptr); m_iblPassSetLayout = nullptr; }
    rscLayout = static_cast<VkDescriptorSetLayout>(m_outlinePostSetLayout);
    if (rscLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, rscLayout, nullptr); m_outlinePostSetLayout = nullptr; }
    if (m_emptyResourceSet) { DestroyDescriptorSet(m_emptyResourceSet); m_emptyResourceSet = nullptr; }
    if (m_emptyResourceSetLayoutWrapper) { DestroyPipelineLayout(m_emptyResourceSetLayoutWrapper); m_emptyResourceSetLayoutWrapper = nullptr; }
    rscLayout = static_cast<VkDescriptorSetLayout>(m_emptyResourceSetLayout);
    if (rscLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, rscLayout, nullptr); m_emptyResourceSetLayout = nullptr; }

    VkPipelineLayout defaultPl = static_cast<VkPipelineLayout>(m_defaultPipelineLayout);
    if (defaultPl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, defaultPl, nullptr);
        m_defaultPipelineLayout = nullptr;
    }
    VkPipelineLayout computePl = static_cast<VkPipelineLayout>(m_computePipelineLayout);
    if (computePl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, computePl, nullptr);
        m_computePipelineLayout = nullptr;
    }
    if (m_legacyComputeParamsUBO) {
        DestroyBuffer(m_legacyComputeParamsUBO);
        m_legacyComputeParamsUBO = nullptr;
    }
    m_computeSet0 = nullptr;  // freed with descriptor pool
    VkDescriptorSetLayout computeSet0Layout = static_cast<VkDescriptorSetLayout>(m_computeSet0Layout);
    if (computeSet0Layout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, computeSet0Layout, nullptr);
        m_computeSet0Layout = nullptr;
    }
    VkDescriptorPool emptyBindlessPool = static_cast<VkDescriptorPool>(m_emptyBindlessPool);
    if (emptyBindlessPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, emptyBindlessPool, nullptr);
        m_emptyBindlessPool = nullptr;
    }
    m_emptyBindlessSet = nullptr;  // freed with pool
    if (m_bindlessPipelineLayoutWrapper) {
        DestroyPipelineLayout(m_bindlessPipelineLayoutWrapper);
        m_bindlessPipelineLayoutWrapper = nullptr;
    }
    VkDescriptorSetLayout bindlessLayout = static_cast<VkDescriptorSetLayout>(m_bindlessSetLayout);
    if (bindlessLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, bindlessLayout, nullptr);
        m_bindlessSetLayout = nullptr;
    }
    VkSemaphore timelineSem = static_cast<VkSemaphore>(m_timelineSemaphore);
    if (timelineSem != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, timelineSem, nullptr);
        m_timelineSemaphore = nullptr;
    }
    DestroyCrossQueueBridge();
    if (device != VK_NULL_HANDLE) {
        SwapchainVK::Destroy(&m_swapchain, device, this);
        VkDescriptorPool imguiPool = static_cast<VkDescriptorPool>(m_imGuiDescriptorPool);
        if (imguiPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, imguiPool, nullptr);
            m_imGuiDescriptorPool = nullptr;
        }
    }
    VkInstance inst = static_cast<VkInstance>(m_instanceVK.GetHandle());
    VkSurfaceKHR surface = static_cast<VkSurfaceKHR>(m_surface);
    if (surface != VK_NULL_HANDLE && inst != VK_NULL_HANDLE) {
        PFN_vkDestroySurfaceKHR pfnDestroySurface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
            vkGetInstanceProcAddr(inst, "vkDestroySurfaceKHR"));
        if (pfnDestroySurface) pfnDestroySurface(inst, surface, nullptr);
        m_surface = nullptr;
    }
    // Flush deletion queue one last time: DestroyBuffer(m_legacyComputeParamsUBO) and DestroyPipelineLayout (etc.) above only enqueue; free all VMA allocations before destroying allocator.
    if (device != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < kMaxFrames; ++i)
            m_deletionQueue.Tick(m_device, GetAllocator(), i);
    }
    RetirePendingTextureWrappers(true);
    UploadHeapVK::Destroy(&m_uploadHeap, GetAllocator());
    AllocatorVK::Destroy(&m_allocatorVK);
    if (device != VK_NULL_HANDLE)
        vkDestroyDevice(device, nullptr);
    InstanceVK::Destroy(&m_instanceVK);
    m_device = nullptr;
    QueuesVK::Destroy(&m_queues, device);
    PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
}

bool DeviceVK::Initialize(void* windowHandle, uint32_t width, uint32_t height) {
    width = std::max(1u, width);
    height = std::max(1u, height);

    if (!InstanceVK::Create(&m_instanceVK)) {
        return false;
    }
    core::Log::InfoF("Vulkan instance API version %u.%u.%u",
                     VK_VERSION_MAJOR(m_instanceVK.apiVersion),
                     VK_VERSION_MINOR(m_instanceVK.apiVersion),
                     VK_VERSION_PATCH(m_instanceVK.apiVersion));
    VkInstance instance = static_cast<VkInstance>(m_instanceVK.GetHandle());

    if (windowHandle && width > 0 && height > 0) {
        auto* glfwWindow = static_cast<GLFWwindow*>(windowHandle);
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkResult surfRes = glfwCreateWindowSurface(instance, glfwWindow, nullptr, &surface);
        if (surfRes == VK_SUCCESS)
            m_surface = surface;
        else
            core::Log::ErrorF("DeviceVK::Initialize: glfwCreateWindowSurface failed (0x%x)", surfRes);
    }

    if (!PhysicalDeviceVK::Select(&m_physicalDeviceVK, instance)) {
        InstanceVK::Destroy(&m_instanceVK);
        return false;
    }
    VkPhysicalDevice physicalDevice = static_cast<VkPhysicalDevice>(m_physicalDeviceVK.GetHandle());

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t graphicsFamily = UINT32_MAX;
    uint32_t computeFamily = UINT32_MAX;
    uint32_t transferFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && graphicsFamily == UINT32_MAX)
            graphicsFamily = i;
        if ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && computeFamily == UINT32_MAX)
            computeFamily = i;
        if ((queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && transferFamily == UINT32_MAX)
            transferFamily = i;
    }
    if (graphicsFamily == UINT32_MAX) {
        core::Log::Error("DeviceVK::Initialize: no graphics queue family");
        return false;
    }
    if (computeFamily == UINT32_MAX) computeFamily = graphicsFamily;
    if (transferFamily == UINT32_MAX) transferFamily = graphicsFamily;

    const bool hasDedicatedComputeFamily = (computeFamily != graphicsFamily);
    core::Log::InfoF("Vulkan queue families: graphics=%u compute=%u transfer=%u dedicatedCompute=%d (present uses graphics queue).",
                     graphicsFamily, computeFamily, transferFamily, hasDedicatedComputeFamily ? 1 : 0);

    VkPhysicalDeviceProperties physicalProps = {};
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalProps);
    VkPhysicalDeviceDescriptorIndexingProperties descriptorIndexingProps = {};
    descriptorIndexingProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
    VkPhysicalDeviceProperties2 physicalProps2 = {};
    physicalProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physicalProps2.pNext = &descriptorIndexingProps;
    vkGetPhysicalDeviceProperties2(physicalDevice, &physicalProps2);
    const uint32_t requiredBindlessSampledImages = BindlessHeapVK::kMaxTextures;
    const uint32_t requiredBindlessStorageBuffers = BindlessHeapVK::kMaxBuffers;
    const uint32_t requiredBindlessPoolDescriptors = requiredBindlessSampledImages + requiredBindlessStorageBuffers;
    if (descriptorIndexingProps.maxDescriptorSetUpdateAfterBindSampledImages < requiredBindlessSampledImages) {
        core::Log::ErrorF(
            "DeviceVK::Initialize: maxDescriptorSetUpdateAfterBindSampledImages (%u) is below required bindless texture capacity (%u).",
            descriptorIndexingProps.maxDescriptorSetUpdateAfterBindSampledImages, requiredBindlessSampledImages);
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    if (descriptorIndexingProps.maxDescriptorSetUpdateAfterBindStorageBuffers < requiredBindlessStorageBuffers) {
        core::Log::ErrorF(
            "DeviceVK::Initialize: maxDescriptorSetUpdateAfterBindStorageBuffers (%u) is below required bindless buffer capacity (%u).",
            descriptorIndexingProps.maxDescriptorSetUpdateAfterBindStorageBuffers, requiredBindlessStorageBuffers);
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    if (descriptorIndexingProps.maxUpdateAfterBindDescriptorsInAllPools < requiredBindlessPoolDescriptors) {
        core::Log::ErrorF(
            "DeviceVK::Initialize: maxUpdateAfterBindDescriptorsInAllPools (%u) is below required bindless descriptor pool capacity (%u).",
            descriptorIndexingProps.maxUpdateAfterBindDescriptorsInAllPools, requiredBindlessPoolDescriptors);
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    const bool instanceSupportsCoreDynamicRendering =
        (VK_VERSION_MAJOR(m_instanceVK.apiVersion) > 1) ||
        (VK_VERSION_MAJOR(m_instanceVK.apiVersion) == 1 && VK_VERSION_MINOR(m_instanceVK.apiVersion) >= 3);
    const bool physicalSupportsCoreDynamicRendering =
        (VK_VERSION_MAJOR(physicalProps.apiVersion) > 1) ||
        (VK_VERSION_MAJOR(physicalProps.apiVersion) == 1 && VK_VERSION_MINOR(physicalProps.apiVersion) >= 3);
    const bool hasCoreDynamicRendering = instanceSupportsCoreDynamicRendering && physicalSupportsCoreDynamicRendering;

    uint32_t availableExtCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtCount, nullptr);
    std::vector<VkExtensionProperties> availableExts(availableExtCount);
    if (availableExtCount > 0)
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtCount, availableExts.data());
    const auto hasDeviceExtension = [&availableExts](const char* extensionName) -> bool {
        return std::any_of(availableExts.begin(), availableExts.end(), [extensionName](const VkExtensionProperties& e) {
            return std::strcmp(e.extensionName, extensionName) == 0;
        });
    };
    const bool hasDynamicRenderingExt = std::any_of(availableExts.begin(), availableExts.end(),
        [](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, "VK_KHR_dynamic_rendering") == 0; });
    const bool hasExtendedDynamicStateExt = std::any_of(availableExts.begin(), availableExts.end(),
        [](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, "VK_EXT_extended_dynamic_state") == 0; });
    const bool hasSynchronization2Ext = std::any_of(availableExts.begin(), availableExts.end(),
        [](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, "VK_KHR_synchronization2") == 0; });
    const bool hasBufferDeviceAddressExt = std::any_of(availableExts.begin(), availableExts.end(),
        [](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, "VK_KHR_buffer_device_address") == 0; });
    const bool hasDescriptorBufferExt = std::any_of(availableExts.begin(), availableExts.end(),
        [](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, "VK_EXT_descriptor_buffer") == 0; });
    const bool hasGraphicsPipelineLibraryExt = std::any_of(availableExts.begin(), availableExts.end(),
        [](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, "VK_EXT_graphics_pipeline_library") == 0; });
    const bool hasPipelineExecutablePropertiesExt = hasDeviceExtension("VK_KHR_pipeline_executable_properties");
    const bool hasDeviceFaultExt = hasDeviceExtension("VK_EXT_device_fault");

#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT)
    if (hasDescriptorBufferExt) {
        VkPhysicalDeviceDescriptorBufferPropertiesEXT dbProps = {};
        dbProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
        VkPhysicalDeviceProperties2 props2 = {};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &dbProps;
        vkGetPhysicalDeviceProperties2(physicalDevice, &props2);
        m_descriptorBufferCombinedImgSize = dbProps.combinedImageSamplerDescriptorSize;
        m_descriptorBufferStorageBufSize = dbProps.storageBufferDescriptorSize;
        if (dbProps.descriptorBufferOffsetAlignment != 0u)
            m_descriptorBufferOffsetAlignment = dbProps.descriptorBufferOffsetAlignment;
    }
#endif

    std::vector<const char*> deviceExtensions = {
        "VK_KHR_swapchain",
    };
    if (!hasCoreDynamicRendering) {
        if (!hasDynamicRenderingExt) {
            core::Log::Error("DeviceVK::Initialize: dynamic rendering required, but neither Vulkan 1.3 nor VK_KHR_dynamic_rendering is available.");
            InstanceVK::Destroy(&m_instanceVK);
            PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
            return false;
        }
        deviceExtensions.push_back("VK_KHR_dynamic_rendering");
    }
    const bool instanceSupportsCoreSynchronization2 =
        (VK_VERSION_MAJOR(m_instanceVK.apiVersion) > 1) ||
        (VK_VERSION_MAJOR(m_instanceVK.apiVersion) == 1 && VK_VERSION_MINOR(m_instanceVK.apiVersion) >= 3);
    const bool physicalSupportsCoreSynchronization2 =
        (VK_VERSION_MAJOR(physicalProps.apiVersion) > 1) ||
        (VK_VERSION_MAJOR(physicalProps.apiVersion) == 1 && VK_VERSION_MINOR(physicalProps.apiVersion) >= 3);
    const bool hasCoreSynchronization2 = instanceSupportsCoreSynchronization2 && physicalSupportsCoreSynchronization2;
    if (!hasCoreSynchronization2) {
        if (!hasSynchronization2Ext) {
            core::Log::Error("DeviceVK::Initialize: synchronization2 required, but neither Vulkan 1.3 nor VK_KHR_synchronization2 is available.");
            InstanceVK::Destroy(&m_instanceVK);
            PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
            return false;
        }
        deviceExtensions.push_back("VK_KHR_synchronization2");
    }
    const bool instanceSupportsCoreBufferDeviceAddress =
        (VK_VERSION_MAJOR(m_instanceVK.apiVersion) > 1) ||
        (VK_VERSION_MAJOR(m_instanceVK.apiVersion) == 1 && VK_VERSION_MINOR(m_instanceVK.apiVersion) >= 2);
    const bool physicalSupportsCoreBufferDeviceAddress =
        (VK_VERSION_MAJOR(physicalProps.apiVersion) > 1) ||
        (VK_VERSION_MAJOR(physicalProps.apiVersion) == 1 && VK_VERSION_MINOR(physicalProps.apiVersion) >= 2);
    const bool hasCoreBufferDeviceAddress = instanceSupportsCoreBufferDeviceAddress && physicalSupportsCoreBufferDeviceAddress;
    if (!hasCoreBufferDeviceAddress && hasBufferDeviceAddressExt) {
        deviceExtensions.push_back("VK_KHR_buffer_device_address");
    }
    if (hasExtendedDynamicStateExt)
        deviceExtensions.push_back("VK_EXT_extended_dynamic_state");
    if (hasDeviceExtension("VK_EXT_swapchain_colorspace")) {
        deviceExtensions.push_back("VK_EXT_swapchain_colorspace");
        m_supportsSwapchainColorspace = true;
    } else {
        m_supportsSwapchainColorspace = false;
    }
    std::vector<const char*> missingRequiredDeviceExtensions;
    for (const char* requiredExtension : deviceExtensions) {
        if (!hasDeviceExtension(requiredExtension))
            missingRequiredDeviceExtensions.push_back(requiredExtension);
    }
    if (!missingRequiredDeviceExtensions.empty()) {
        std::string missingJoined;
        for (size_t i = 0; i < missingRequiredDeviceExtensions.size(); ++i) {
            if (i) missingJoined += ", ";
            missingJoined += missingRequiredDeviceExtensions[i];
        }
        core::Log::ErrorF("DeviceVK::Initialize: missing required device extension(s): %s", missingJoined.c_str());
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }

    float queuePriority = 1.0f;
    std::set<uint32_t> uniqueFamilies = { graphicsFamily, computeFamily, transferFamily };
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t qf : uniqueFamilies) {
        VkDeviceQueueCreateInfo qci = {};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = qf;
        qci.queueCount = 1;
        qci.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(qci);
    }

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features.fragmentStoresAndAtomics = VK_TRUE;

    // Vulkan 1.2+ / 1.3+: promoted feature structs must not duplicate VkPhysicalDeviceVulkan12Features /
    // VkPhysicalDeviceVulkan13Features (VUID-VkDeviceCreateInfo-pNext-02830 and related).
    const bool chainVulkan12 =
        (VK_VERSION_MAJOR(physicalProps.apiVersion) > 1u) ||
        (VK_VERSION_MAJOR(physicalProps.apiVersion) == 1u && VK_VERSION_MINOR(physicalProps.apiVersion) >= 2u);
    const bool chainVulkan13 =
        (VK_VERSION_MAJOR(physicalProps.apiVersion) > 1u) ||
        (VK_VERSION_MAJOR(physicalProps.apiVersion) == 1u && VK_VERSION_MINOR(physicalProps.apiVersion) >= 3u);

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing = {};
    descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures = {};
    timelineSemaphoreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering = {};
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    VkPhysicalDeviceSynchronization2Features synchronization2 = {};
    synchronization2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    VkPhysicalDeviceVulkan11Features vulkan11Features = {};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    VkPhysicalDeviceVulkan12Features vulkan12Features = {};
#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
    VkPhysicalDeviceVulkan13Features vulkan13Features = {};
#endif

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicState = {};
    extendedDynamicState.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
    VkPhysicalDeviceHostQueryResetFeatures hostQueryResetFeatures = {};
    hostQueryResetFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

    if (chainVulkan12) {
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vulkan12Features.descriptorIndexing = VK_TRUE;
        vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
        vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
        vulkan12Features.timelineSemaphore = VK_TRUE;
        vulkan12Features.hostQueryReset = VK_TRUE;
        vulkan12Features.bufferDeviceAddress = VK_TRUE;

        features2.pNext = &vulkan11Features;
        vulkan11Features.pNext = &vulkan12Features;

#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
        if (chainVulkan13 && hasCoreDynamicRendering && hasCoreSynchronization2) {
            vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            vulkan13Features.dynamicRendering = VK_TRUE;
            vulkan13Features.synchronization2 = VK_TRUE;
            vulkan12Features.pNext = &vulkan13Features;
            if (hasExtendedDynamicStateExt) {
                vulkan13Features.pNext = &extendedDynamicState;
                extendedDynamicState.pNext = nullptr;
            } else {
                vulkan13Features.pNext = nullptr;
            }
        } else
#endif
        {
            dynamicRendering.dynamicRendering = VK_TRUE;
            synchronization2.synchronization2 = VK_TRUE;
            vulkan12Features.pNext = &dynamicRendering;
            dynamicRendering.pNext = &synchronization2;
            if (hasExtendedDynamicStateExt) {
                synchronization2.pNext = &extendedDynamicState;
                extendedDynamicState.pNext = nullptr;
            } else {
                synchronization2.pNext = nullptr;
            }
        }
    } else {
        descriptorIndexing.descriptorBindingPartiallyBound = VK_TRUE;
        descriptorIndexing.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        descriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
        features2.pNext = &descriptorIndexing;
        timelineSemaphoreFeatures.timelineSemaphore = VK_TRUE;
        descriptorIndexing.pNext = &timelineSemaphoreFeatures;
        dynamicRendering.dynamicRendering = VK_TRUE;
        timelineSemaphoreFeatures.pNext = &dynamicRendering;
        synchronization2.synchronization2 = VK_TRUE;
        dynamicRendering.pNext = &synchronization2;
        synchronization2.pNext = &vulkan11Features;
        vulkan11Features.pNext = nullptr;
        if (hasExtendedDynamicStateExt)
            vulkan11Features.pNext = &extendedDynamicState;
        else
            vulkan11Features.pNext = &hostQueryResetFeatures;
        if (hasExtendedDynamicStateExt)
            extendedDynamicState.pNext = &hostQueryResetFeatures;
        hostQueryResetFeatures.pNext = &bufferDeviceAddressFeatures;
        bufferDeviceAddressFeatures.pNext = nullptr;
    }

    bool probeSwapchainMaint1 = false;
#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT)
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchainMaint1Features = {};
    probeSwapchainMaint1 = hasDeviceExtension("VK_EXT_swapchain_maintenance1");
    if (probeSwapchainMaint1) {
        swapchainMaint1Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
        swapchainMaint1Features.pNext = nullptr;
        if (chainVulkan12) {
#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
            if (chainVulkan13 && hasCoreDynamicRendering && hasCoreSynchronization2) {
                if (hasExtendedDynamicStateExt)
                    extendedDynamicState.pNext = &swapchainMaint1Features;
                else
                    vulkan13Features.pNext = &swapchainMaint1Features;
            } else
#endif
            {
                if (hasExtendedDynamicStateExt)
                    extendedDynamicState.pNext = &swapchainMaint1Features;
                else
                    synchronization2.pNext = &swapchainMaint1Features;
            }
        } else {
            bufferDeviceAddressFeatures.pNext = &swapchainMaint1Features;
        }
    }
#endif

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT)
    if (probeSwapchainMaint1 && swapchainMaint1Features.swapchainMaintenance1 == VK_TRUE) {
        deviceExtensions.push_back("VK_EXT_swapchain_maintenance1");
        swapchainMaint1Features.swapchainMaintenance1 = VK_TRUE;
        m_supportsSwapchainMaintenance1 = true;
    } else {
        if (probeSwapchainMaint1) {
            if (!chainVulkan12) {
                bufferDeviceAddressFeatures.pNext = nullptr;
            } else {
#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
                if (chainVulkan13 && hasCoreDynamicRendering && hasCoreSynchronization2) {
                    if (hasExtendedDynamicStateExt)
                        extendedDynamicState.pNext = nullptr;
                    else
                        vulkan13Features.pNext = nullptr;
                } else
#endif
                {
                    if (hasExtendedDynamicStateExt)
                        extendedDynamicState.pNext = nullptr;
                    else
                        synchronization2.pNext = nullptr;
                }
            }
        }
        m_supportsSwapchainMaintenance1 = false;
    }
#else
    m_supportsSwapchainMaintenance1 = false;
#endif
    if (features2.features.fragmentStoresAndAtomics != VK_TRUE) {
        core::Log::Error("DeviceVK::Initialize: fragmentStoresAndAtomics is not supported by the selected physical device.");
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    const VkBool32 partiallyBound =
        chainVulkan12 ? vulkan12Features.descriptorBindingPartiallyBound : descriptorIndexing.descriptorBindingPartiallyBound;
    const VkBool32 sampledUab =
        chainVulkan12 ? vulkan12Features.descriptorBindingSampledImageUpdateAfterBind
                      : descriptorIndexing.descriptorBindingSampledImageUpdateAfterBind;
    const VkBool32 storageUab =
        chainVulkan12 ? vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind
                      : descriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind;
    if (partiallyBound != VK_TRUE) {
        core::Log::Error("DeviceVK::Initialize: descriptorBindingPartiallyBound is not supported; required by bindless descriptor path.");
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    if (sampledUab != VK_TRUE) {
        core::Log::Error("DeviceVK::Initialize: descriptorBindingSampledImageUpdateAfterBind is not supported; required by bindless descriptor path.");
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    if (storageUab != VK_TRUE) {
        core::Log::Error("DeviceVK::Initialize: descriptorBindingStorageBufferUpdateAfterBind is not supported; required by bindless descriptor path.");
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    const VkBool32 timelineOk = chainVulkan12 ? vulkan12Features.timelineSemaphore : timelineSemaphoreFeatures.timelineSemaphore;
    if (timelineOk != VK_TRUE) {
        core::Log::Error("DeviceVK::Initialize: timelineSemaphore is not supported by the selected physical device.");
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
    const bool useVulkan13DynamicAndSync2 =
        chainVulkan12 && chainVulkan13 && hasCoreDynamicRendering && hasCoreSynchronization2;
    const VkBool32 dynamicRenderingOk =
        useVulkan13DynamicAndSync2 ? vulkan13Features.dynamicRendering : dynamicRendering.dynamicRendering;
    const VkBool32 synchronization2Ok =
        useVulkan13DynamicAndSync2 ? vulkan13Features.synchronization2 : synchronization2.synchronization2;
#else
    const VkBool32 dynamicRenderingOk = dynamicRendering.dynamicRendering;
    const VkBool32 synchronization2Ok = synchronization2.synchronization2;
#endif
    if (dynamicRenderingOk != VK_TRUE) {
        core::Log::Error("DeviceVK::Initialize: dynamicRendering feature is not supported by the selected physical device.");
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    if (synchronization2Ok != VK_TRUE) {
        core::Log::Error("DeviceVK::Initialize: synchronization2 feature is not supported by the selected physical device.");
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    if (vulkan11Features.shaderDrawParameters != VK_TRUE) {
        core::Log::Error("DeviceVK::Initialize: shaderDrawParameters (Vulkan 1.1) is not supported; required for gl_BaseVertex/gl_BaseInstance in shaders.");
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_supportsExtendedDynamicState = hasExtendedDynamicStateExt && (extendedDynamicState.extendedDynamicState == VK_TRUE);
    core::Log::InfoF("Vulkan features: extendedDynamicState extPresent=%d enabled=%d",
                     hasExtendedDynamicStateExt ? 1 : 0,
                     m_supportsExtendedDynamicState ? 1 : 0);

    m_supportsDrawIndirectCount = false;
    if (chainVulkan12) {
        if (vulkan12Features.drawIndirectCount == VK_TRUE) {
            m_supportsDrawIndirectCount = true;
            vulkan12Features.drawIndirectCount = VK_TRUE;
        } else {
            vulkan12Features.drawIndirectCount = VK_FALSE;
        }
    }
    core::Log::InfoF("Vulkan: drawIndirectCount (1.2) supported=%d", m_supportsDrawIndirectCount ? 1 : 0);

    m_supportsTessellationShader = (features2.features.tessellationShader == VK_TRUE);
    m_supportsGeometryShader = (features2.features.geometryShader == VK_TRUE);
    // Vulkan 1.4+ headers omit `occlusionQuery` from `VkPhysicalDeviceFeatures` (baseline / promoted);
    // `VK_QUERY_TYPE_OCCLUSION` remains in core. Enable the cap for the standard desktop Vulkan path.
    m_supportsOcclusionQuery = true;
    m_supportsHostQueryReset =
        chainVulkan12 ? (vulkan12Features.hostQueryReset == VK_TRUE) : (hostQueryResetFeatures.hostQueryReset == VK_TRUE);
    if (m_supportsHostQueryReset) {
        if (chainVulkan12)
            vulkan12Features.hostQueryReset = VK_TRUE;
        else
            hostQueryResetFeatures.hostQueryReset = VK_TRUE;
    } else {
        if (chainVulkan12)
            vulkan12Features.hostQueryReset = VK_FALSE;
        else
            hostQueryResetFeatures.hostQueryReset = VK_FALSE;
    }
    m_supportsPipelineStatisticsQuery = (features2.features.pipelineStatisticsQuery == VK_TRUE);
    if (m_supportsPipelineStatisticsQuery)
        features2.features.pipelineStatisticsQuery = VK_TRUE;
    else
        features2.features.pipelineStatisticsQuery = VK_FALSE;
    m_supportsBufferDeviceAddress = chainVulkan12 ? (vulkan12Features.bufferDeviceAddress == VK_TRUE)
                                                 : (bufferDeviceAddressFeatures.bufferDeviceAddress == VK_TRUE);
    if (m_supportsBufferDeviceAddress) {
        if (chainVulkan12)
            vulkan12Features.bufferDeviceAddress = VK_TRUE;
        else
            bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
    } else {
        if (chainVulkan12)
            vulkan12Features.bufferDeviceAddress = VK_FALSE;
        else
            bufferDeviceAddressFeatures.bufferDeviceAddress = VK_FALSE;
    }
    m_supportsSynchronization2 = true;
#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
    if (chainVulkan12 && chainVulkan13 && hasCoreDynamicRendering && hasCoreSynchronization2)
        vulkan13Features.synchronization2 = VK_TRUE;
    else
#endif
        synchronization2.synchronization2 = VK_TRUE;

#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT)
    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures = {};
    descriptorBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    bool descriptorBufferSupported = false;
    if (hasDescriptorBufferExt) {
        VkPhysicalDeviceFeatures2 descriptorBufferProbe = {};
        descriptorBufferProbe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        descriptorBufferProbe.pNext = &descriptorBufferFeatures;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &descriptorBufferProbe);
        descriptorBufferSupported = (descriptorBufferFeatures.descriptorBuffer == VK_TRUE);
    }
    if (descriptorBufferSupported) {
        deviceExtensions.push_back("VK_EXT_descriptor_buffer");
        descriptorBufferFeatures.descriptorBuffer = VK_TRUE;
        VkBaseOutStructure* tail = reinterpret_cast<VkBaseOutStructure*>(&features2);
        while (tail->pNext) tail = reinterpret_cast<VkBaseOutStructure*>(tail->pNext);
        tail->pNext = reinterpret_cast<VkBaseOutStructure*>(&descriptorBufferFeatures);
        m_supportsDescriptorBuffer = true;
    } else {
        m_supportsDescriptorBuffer = false;
    }
#else
    (void)hasDescriptorBufferExt;
    m_supportsDescriptorBuffer = false;
#endif

#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT)
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphicsPipelineLibraryFeatures = {};
    graphicsPipelineLibraryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
    bool graphicsPipelineLibrarySupported = false;
    if (hasGraphicsPipelineLibraryExt) {
        VkPhysicalDeviceFeatures2 gplProbe = {};
        gplProbe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        gplProbe.pNext = &graphicsPipelineLibraryFeatures;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &gplProbe);
        graphicsPipelineLibrarySupported = (graphicsPipelineLibraryFeatures.graphicsPipelineLibrary == VK_TRUE);
    }
    if (graphicsPipelineLibrarySupported) {
        deviceExtensions.push_back("VK_EXT_graphics_pipeline_library");
        graphicsPipelineLibraryFeatures.graphicsPipelineLibrary = VK_TRUE;
        {
            VkBaseOutStructure* tail = reinterpret_cast<VkBaseOutStructure*>(&features2);
            while (tail->pNext) tail = reinterpret_cast<VkBaseOutStructure*>(tail->pNext);
            tail->pNext = reinterpret_cast<VkBaseOutStructure*>(&graphicsPipelineLibraryFeatures);
        }
        m_supportsGraphicsPipelineLibrary = true;
    } else {
        m_supportsGraphicsPipelineLibrary = false;
    }
#else
    (void)hasGraphicsPipelineLibraryExt;
    m_supportsGraphicsPipelineLibrary = false;
#endif

#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR) && defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR)
    VkPhysicalDevicePresentIdFeaturesKHR presentIdFeaturesKHR = {};
    VkPhysicalDevicePresentWaitFeaturesKHR presentWaitFeaturesKHR = {};
    presentIdFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;
    presentWaitFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
    presentIdFeaturesKHR.pNext = &presentWaitFeaturesKHR;
    presentWaitFeaturesKHR.pNext = nullptr;
    VkPhysicalDeviceFeatures2 presentProbe = {};
    presentProbe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    presentProbe.pNext = &presentIdFeaturesKHR;
    vkGetPhysicalDeviceFeatures2(physicalDevice, &presentProbe);

    const bool wantPresentWait =
        hasDeviceExtension("VK_KHR_present_id") && hasDeviceExtension("VK_KHR_present_wait") &&
        presentIdFeaturesKHR.presentId == VK_TRUE && presentWaitFeaturesKHR.presentWait == VK_TRUE;
    if (wantPresentWait) {
        deviceExtensions.push_back("VK_KHR_present_id");
        deviceExtensions.push_back("VK_KHR_present_wait");
        presentIdFeaturesKHR.presentId = VK_TRUE;
        presentWaitFeaturesKHR.presentWait = VK_TRUE;
        m_supportsPresentWait = true;
        {
            VkBaseOutStructure* tail = reinterpret_cast<VkBaseOutStructure*>(&features2);
            while (tail->pNext) tail = reinterpret_cast<VkBaseOutStructure*>(tail->pNext);
            tail->pNext = reinterpret_cast<VkBaseOutStructure*>(&presentIdFeaturesKHR);
        }
    } else {
        m_supportsPresentWait = false;
    }
#else
    m_supportsPresentWait = false;
#endif

#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR)
    VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR pipelineExecutablePropertiesFeatures = {};
    pipelineExecutablePropertiesFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR;
    bool pipelineExecutablePropertiesSupported = false;
    if (hasPipelineExecutablePropertiesExt) {
        VkPhysicalDeviceFeatures2 pepProbe = {};
        pepProbe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        pepProbe.pNext = &pipelineExecutablePropertiesFeatures;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &pepProbe);
        pipelineExecutablePropertiesSupported = (pipelineExecutablePropertiesFeatures.pipelineExecutableProperties == VK_TRUE);
    }
    if (pipelineExecutablePropertiesSupported) {
        deviceExtensions.push_back("VK_KHR_pipeline_executable_properties");
        pipelineExecutablePropertiesFeatures.pipelineExecutableProperties = VK_TRUE;
        {
            VkBaseOutStructure* tail = reinterpret_cast<VkBaseOutStructure*>(&features2);
            while (tail->pNext) tail = reinterpret_cast<VkBaseOutStructure*>(tail->pNext);
            tail->pNext = reinterpret_cast<VkBaseOutStructure*>(&pipelineExecutablePropertiesFeatures);
        }
        m_supportsPipelineExecutableProperties = true;
    } else {
        m_supportsPipelineExecutableProperties = false;
    }
#else
    (void)hasPipelineExecutablePropertiesExt;
    m_supportsPipelineExecutableProperties = false;
#endif

#if defined(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT)
    VkPhysicalDeviceFaultFeaturesEXT faultFeatures = {};
    faultFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT;
    bool deviceFaultSupported = false;
    if (hasDeviceFaultExt) {
        VkPhysicalDeviceFeatures2 faultProbe = {};
        faultProbe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        faultProbe.pNext = &faultFeatures;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &faultProbe);
        deviceFaultSupported = (faultFeatures.deviceFault == VK_TRUE);
    }
    if (deviceFaultSupported) {
        deviceExtensions.push_back("VK_EXT_device_fault");
        faultFeatures.deviceFault = VK_TRUE;
        {
            VkBaseOutStructure* tail = reinterpret_cast<VkBaseOutStructure*>(&features2);
            while (tail->pNext) tail = reinterpret_cast<VkBaseOutStructure*>(tail->pNext);
            tail->pNext = reinterpret_cast<VkBaseOutStructure*>(&faultFeatures);
        }
        m_supportsDeviceFault = true;
    } else {
        m_supportsDeviceFault = false;
    }
#else
    (void)hasDeviceFaultExt;
    m_supportsDeviceFault = false;
#endif

    core::Log::InfoF(
        "Vulkan required feature status: fragmentStoresAndAtomics=%d descriptorPartiallyBound=%d descriptorSampledImageUpdateAfterBind=%d descriptorStorageBufferUpdateAfterBind=%d timelineSemaphore=%d dynamicRendering=%d synchronization2=%d shaderDrawParameters=%d",
        1, 1, 1, 1, 1, 1, 1, 1);
    core::Log::InfoF(
        "Vulkan optional feature status: drawIndirectCount=%d extendedDynamicState=%d hostQueryReset=%d pipelineStatisticsQuery=%d bufferDeviceAddress=%d descriptorBuffer=%d graphicsPipelineLibrary=%d tessellationShader=%d geometryShader=%d occlusionQueries=%d swapchainMaintenance1=%d swapchainColorspace=%d presentWait=%d pipelineExecutableProperties=%d deviceFault=%d",
        m_supportsDrawIndirectCount ? 1 : 0, m_supportsExtendedDynamicState ? 1 : 0,
        m_supportsHostQueryReset ? 1 : 0, m_supportsPipelineStatisticsQuery ? 1 : 0,
        m_supportsBufferDeviceAddress ? 1 : 0, m_supportsDescriptorBuffer ? 1 : 0, m_supportsGraphicsPipelineLibrary ? 1 : 0,
        m_supportsTessellationShader ? 1 : 0,
        m_supportsGeometryShader ? 1 : 0, m_supportsOcclusionQuery ? 1 : 0, m_supportsSwapchainMaintenance1 ? 1 : 0,
        m_supportsSwapchainColorspace ? 1 : 0, m_supportsPresentWait ? 1 : 0, m_supportsPipelineExecutableProperties ? 1 : 0, m_supportsDeviceFault ? 1 : 0);

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &features2;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device = VK_NULL_HANDLE;
    VkResult res = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateDevice failed (0x%x)", res);
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_device = device;
    SetDebugNameDevice(this, "OpenEye_VkDevice");
    {
        std::string extJoined;
        for (size_t i = 0; i < deviceExtensions.size(); ++i) {
            if (i) extJoined += ", ";
            extJoined += deviceExtensions[i];
        }
        core::Log::InfoF("Vulkan logical device extensions (%u): %s",
                         static_cast<uint32_t>(deviceExtensions.size()), extJoined.c_str());
    }
    if (m_supportsSwapchainMaintenance1) {
        m_pfnReleaseSwapchainImagesEXT =
            reinterpret_cast<PFN_vkReleaseSwapchainImagesEXT>(vkGetDeviceProcAddr(device, "vkReleaseSwapchainImagesEXT"));
        if (!m_pfnReleaseSwapchainImagesEXT)
            core::Log::Warn("Vulkan: VK_EXT_swapchain_maintenance1 enabled but vkReleaseSwapchainImagesEXT was not loaded.");
    }
    if (m_supportsPresentWait) {
        m_pfnWaitForPresentKHR = reinterpret_cast<void*>(vkGetDeviceProcAddr(device, "vkWaitForPresentKHR"));
        if (!m_pfnWaitForPresentKHR)
            core::Log::Warn("Vulkan: present wait enabled but vkWaitForPresentKHR was not loaded.");
    }
    if (m_supportsDescriptorBuffer) {
        m_pfnGetDescriptorSetLayoutSizeEXT = reinterpret_cast<void*>(vkGetDeviceProcAddr(device, "vkGetDescriptorSetLayoutSizeEXT"));
        m_pfnGetDescriptorSetLayoutBindingOffsetEXT =
            reinterpret_cast<void*>(vkGetDeviceProcAddr(device, "vkGetDescriptorSetLayoutBindingOffsetEXT"));
        m_pfnGetDescriptorEXT = reinterpret_cast<void*>(vkGetDeviceProcAddr(device, "vkGetDescriptorEXT"));
        m_pfnCmdBindDescriptorBuffersEXT = reinterpret_cast<void*>(vkGetDeviceProcAddr(device, "vkCmdBindDescriptorBuffersEXT"));
        m_pfnCmdSetDescriptorBufferOffsetsEXT =
            reinterpret_cast<void*>(vkGetDeviceProcAddr(device, "vkCmdSetDescriptorBufferOffsetsEXT"));
        const bool missingRequiredDescriptorBufferEntryPoints =
            !m_pfnGetDescriptorSetLayoutSizeEXT || !m_pfnGetDescriptorSetLayoutBindingOffsetEXT || !m_pfnGetDescriptorEXT ||
            !m_pfnCmdBindDescriptorBuffersEXT || !m_pfnCmdSetDescriptorBufferOffsetsEXT;
        if (missingRequiredDescriptorBufferEntryPoints) {
            core::Log::Warn("Vulkan: descriptor buffer extension enabled but one or more entry points were not loaded; disabling descriptor-buffer capability.");
            m_supportsDescriptorBuffer = false;
            m_pfnGetDescriptorSetLayoutSizeEXT = nullptr;
            m_pfnGetDescriptorSetLayoutBindingOffsetEXT = nullptr;
            m_pfnGetDescriptorEXT = nullptr;
            m_pfnCmdBindDescriptorBuffersEXT = nullptr;
            m_pfnCmdSetDescriptorBufferOffsetsEXT = nullptr;
        }
    }

    if (!QueuesVK::Create(&m_queues, device, graphicsFamily, computeFamily, transferFamily)) {
        core::Log::Error("DeviceVK::Initialize: QueuesVK::Create failed");
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }

    if (!AllocatorVK::Create(&m_allocatorVK, physicalDevice, device, instance, m_instanceVK.apiVersion, m_supportsBufferDeviceAddress)) {
        core::Log::Error("DeviceVK::Initialize: AllocatorVK::Create failed");
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }

    for (uint32_t i = 0; i < kMaxFrames; ++i) {
        if (!FrameContextVK::Create(&m_frameContexts[i], device, m_queues.GetGraphicsQueueFamily())) {
            core::Log::ErrorF("DeviceVK::Initialize: FrameContextVK::Create failed for frame %u", i);
            while (i > 0) {
                FrameContextVK::Destroy(&m_frameContexts[--i], device);
            }
            AllocatorVK::Destroy(&m_allocatorVK);
            vkDestroyDevice(device, nullptr);
            InstanceVK::Destroy(&m_instanceVK);
            m_device = nullptr;
            PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
            return false;
        }
    }

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCreateInfo.maxSets = kMainDescriptorPoolMaxSets;
    poolCreateInfo.poolSizeCount = kMainDescriptorPoolSizeCount;
    poolCreateInfo.pPoolSizes = kMainDescriptorPoolSizes;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    res = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateDescriptorPool failed (0x%x)", res);
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_descriptorPool = descriptorPool;

    if (!DescriptorAllocatorVK::Create(&m_descriptorAllocator, m_device, kDescriptorPoolSizes, kDescriptorPoolSizeCount, kDescriptorMaxSetsPerPool)) {
        core::Log::Error("DeviceVK::Initialize: DescriptorAllocatorVK::Create failed");
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_descriptorPool = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    if (!DescriptorCacheVK::Create(&m_descriptorCache, m_device, kDescriptorPoolSizes, kDescriptorPoolSizeCount, kDescriptorMaxSetsPerPool)) {
        core::Log::Error("DeviceVK::Initialize: DescriptorCacheVK::Create failed");
        DescriptorAllocatorVK::Destroy(&m_descriptorAllocator, device);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_descriptorPool = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    VkSampler defaultSampler = VK_NULL_HANDLE;
    res = vkCreateSampler(device, &samplerInfo, nullptr, &defaultSampler);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateSampler failed (0x%x)", res);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_descriptorPool = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_defaultSampler = defaultSampler;

    std::vector<uint8_t> pipelineCacheInitialData;
    VkPhysicalDevice phys = static_cast<VkPhysicalDevice>(m_physicalDeviceVK.GetHandle());
    std::string pipelineCachePath = GetPipelineCachePathForPhysicalDevice(phys);
    if (!pipelineCachePath.empty() && eyelib::platform::filesystem::FileExists(pipelineCachePath))
        pipelineCacheInitialData = eyelib::platform::filesystem::ReadFile(pipelineCachePath);
    VkPipelineCacheCreateInfo pipelineCacheInfo = {};
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCacheInfo.initialDataSize = pipelineCacheInitialData.size();
    pipelineCacheInfo.pInitialData = pipelineCacheInitialData.empty() ? nullptr : pipelineCacheInitialData.data();
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    res = vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr, &pipelineCache);
    if (res != VK_SUCCESS && !pipelineCacheInitialData.empty()) {
        pipelineCacheInfo.initialDataSize = 0;
        pipelineCacheInfo.pInitialData = nullptr;
        res = vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr, &pipelineCache);
    }
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreatePipelineCache failed (0x%x)", res);
        vkDestroySampler(device, defaultSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_defaultSampler = nullptr;
        m_descriptorPool = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_pipelineCache = pipelineCache;

    VkDescriptorSetLayout computeSet0Layout = VK_NULL_HANDLE;
    VkDescriptorSet computeSet0 = VK_NULL_HANDLE;
    if (!vkhelpers::CreateComputeSet0(device, descriptorPool, &computeSet0Layout, &computeSet0)) {
        core::Log::Error("DeviceVK::Initialize: failed to create compute set0 resources");
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
        vkDestroySampler(device, defaultSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_descriptorPool = nullptr;
        m_defaultSampler = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_computeSet0Layout = computeSet0Layout;
    m_computeSet0 = computeSet0;

    constexpr uint32_t kMaxBindlessTextures = 8192;
    constexpr uint32_t kMaxBindlessBuffers = 8192;
    VkDescriptorBindingFlagsEXT bindlessFlags[] = {
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
    };
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsInfo = {};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
    bindingFlagsInfo.bindingCount = 2;
    bindingFlagsInfo.pBindingFlags = bindlessFlags;
    VkDescriptorSetLayoutBinding bindlessBindings[2] = {};
    bindlessBindings[0].binding = 0;
    bindlessBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindlessBindings[0].descriptorCount = kMaxBindlessTextures;
    bindlessBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindlessBindings[0].pImmutableSamplers = nullptr;
    bindlessBindings[1].binding = 1;
    bindlessBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindlessBindings[1].descriptorCount = kMaxBindlessBuffers;
    bindlessBindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindlessBindings[1].pImmutableSamplers = nullptr;
    VkDescriptorSetLayoutCreateInfo bindlessLayoutInfo = {};
    bindlessLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    bindlessLayoutInfo.pNext = &bindingFlagsInfo;
    bindlessLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
#if defined(VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
    if (m_supportsDescriptorBuffer)
        bindlessLayoutInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
#endif
    bindlessLayoutInfo.bindingCount = 2;
    bindlessLayoutInfo.pBindings = bindlessBindings;
    VkDescriptorSetLayout bindlessSetLayout = VK_NULL_HANDLE;
    res = vkCreateDescriptorSetLayout(device, &bindlessLayoutInfo, nullptr, &bindlessSetLayout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateDescriptorSetLayout (bindless) failed (0x%x)", res);
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
        vkDestroySampler(device, defaultSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_defaultSampler = nullptr;
        m_descriptorPool = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_bindlessSetLayout = bindlessSetLayout;

    VkPipelineLayout defaultPipelineLayout = vkres::CreatePushOnlyPipelineLayout(device);
    if (defaultPipelineLayout == VK_NULL_HANDLE) {
        core::Log::Error("DeviceVK::Initialize: failed to create push-only pipeline layout");
        vkDestroyDescriptorSetLayout(device, bindlessSetLayout, nullptr);
        m_bindlessSetLayout = nullptr;
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
        vkDestroySampler(device, defaultSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_defaultSampler = nullptr;
        m_descriptorPool = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_defaultPipelineLayout = defaultPipelineLayout;

    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
    if (!vkhelpers::CreateComputePipelineLayout(device, computeSet0Layout, bindlessSetLayout, m_supportsDescriptorBuffer,
                                                &computePipelineLayout)) {
        core::Log::Error("DeviceVK::Initialize: failed to create compute pipeline layout");
        vkDestroyPipelineLayout(device, defaultPipelineLayout, nullptr);
        m_defaultPipelineLayout = nullptr;
        vkDestroyDescriptorSetLayout(device, bindlessSetLayout, nullptr);
        m_bindlessSetLayout = nullptr;
        vkDestroyDescriptorSetLayout(device, computeSet0Layout, nullptr);
        m_computeSet0Layout = nullptr;
        m_computeSet0 = nullptr;
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
        vkDestroySampler(device, defaultSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_defaultSampler = nullptr;
        m_descriptorPool = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_computePipelineLayout = computePipelineLayout;

    BufferDesc paramsUboDesc;
    paramsUboDesc.sizeInBytes = 256;
    paramsUboDesc.usage = BufferUsage::Uniform;
    paramsUboDesc.memoryHint = BufferMemoryHint::HostVisible;
    m_legacyComputeParamsUBO = CreateBuffer(paramsUboDesc, nullptr);
    if (!m_legacyComputeParamsUBO) {
        core::Log::Warn("DeviceVK::Initialize: failed to create legacy compute params UBO");
    }

    VkDescriptorPoolSize emptyBindlessPoolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxBindlessTextures },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kMaxBindlessBuffers },
    };
    VkDescriptorPoolCreateInfo emptyBindlessPoolInfo = {};
    emptyBindlessPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    emptyBindlessPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    emptyBindlessPoolInfo.maxSets = 1;
    emptyBindlessPoolInfo.poolSizeCount = 2;
    emptyBindlessPoolInfo.pPoolSizes = emptyBindlessPoolSizes;
    VkDescriptorPool emptyBindlessPool = VK_NULL_HANDLE;
    res = vkCreateDescriptorPool(device, &emptyBindlessPoolInfo, nullptr, &emptyBindlessPool);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateDescriptorPool (empty bindless) failed (0x%x)", res);
        vkDestroyPipelineLayout(device, defaultPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, bindlessSetLayout, nullptr);
        m_defaultPipelineLayout = nullptr;
        m_bindlessSetLayout = nullptr;
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
        vkDestroySampler(device, defaultSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_defaultSampler = nullptr;
        m_descriptorPool = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_emptyBindlessPool = emptyBindlessPool;
    VkDescriptorSetAllocateInfo emptyBindlessAlloc = {};
    emptyBindlessAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    emptyBindlessAlloc.descriptorPool = emptyBindlessPool;
    emptyBindlessAlloc.descriptorSetCount = 1;
    emptyBindlessAlloc.pSetLayouts = &bindlessSetLayout;
    VkDescriptorSet emptyBindlessSet = VK_NULL_HANDLE;
    res = vkAllocateDescriptorSets(device, &emptyBindlessAlloc, &emptyBindlessSet);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkAllocateDescriptorSets (empty bindless) failed (0x%x)", res);
        vkDestroyDescriptorPool(device, emptyBindlessPool, nullptr);
        vkDestroyPipelineLayout(device, defaultPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, bindlessSetLayout, nullptr);
        m_emptyBindlessPool = nullptr;
        m_defaultPipelineLayout = nullptr;
        m_bindlessSetLayout = nullptr;
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
        vkDestroySampler(device, defaultSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_defaultSampler = nullptr;
        m_descriptorPool = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_emptyBindlessSet = emptyBindlessSet;

    // Resource-space descriptor set layouts and pipeline layouts (Frame/Pass/Material/SceneBuffers). See descriptor_binding_table.md.
    {
        VkDescriptorSetLayout frameLayout = vkres::CreateFrameSetLayout(device);
        VkDescriptorSetLayout forwardPassLayout = vkres::CreateForwardPassSetLayout(device);
        VkDescriptorSetLayout postPassLayout = vkres::CreatePostPassSetLayout(device);
        VkDescriptorSetLayout shadowPassLayout = vkres::CreateShadowPassSetLayout(device);
        VkDescriptorSetLayout lightCullPassLayout = vkres::CreateLightCullPassSetLayout(device);
        VkDescriptorSetLayout deferredLightingPassLayout = vkres::CreateDeferredLightingPassSetLayout(device);
        VkDescriptorSetLayout materialLayout = vkres::CreateMaterialSetLayout(device);
        VkDescriptorSetLayout sceneLayout = vkres::CreateSceneBuffersSetLayout(device);
        VkDescriptorSetLayout iblPassLayout = vkres::CreateIBLPassSetLayout(device);
        VkDescriptorSetLayout outlinePostLayout = vkres::CreateOutlinePostSetLayout(device);
        VkDescriptorSetLayout emptyLayout = vkres::CreateEmptySetLayout(device);
        if (!frameLayout || !forwardPassLayout || !postPassLayout || !shadowPassLayout || !lightCullPassLayout || !deferredLightingPassLayout || !materialLayout || !sceneLayout || !iblPassLayout || !outlinePostLayout || !emptyLayout) {
            if (emptyLayout) vkDestroyDescriptorSetLayout(device, emptyLayout, nullptr);
            if (outlinePostLayout) vkDestroyDescriptorSetLayout(device, outlinePostLayout, nullptr);
            if (iblPassLayout) vkDestroyDescriptorSetLayout(device, iblPassLayout, nullptr);
            if (sceneLayout) vkDestroyDescriptorSetLayout(device, sceneLayout, nullptr);
            if (materialLayout) vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
            if (deferredLightingPassLayout) vkDestroyDescriptorSetLayout(device, deferredLightingPassLayout, nullptr);
            if (lightCullPassLayout) vkDestroyDescriptorSetLayout(device, lightCullPassLayout, nullptr);
            if (shadowPassLayout) vkDestroyDescriptorSetLayout(device, shadowPassLayout, nullptr);
            if (postPassLayout) vkDestroyDescriptorSetLayout(device, postPassLayout, nullptr);
            if (forwardPassLayout) vkDestroyDescriptorSetLayout(device, forwardPassLayout, nullptr);
            if (frameLayout) vkDestroyDescriptorSetLayout(device, frameLayout, nullptr);
            core::Log::Error("DeviceVK::Initialize: resource-space set layout creation failed");
            vkDestroyDescriptorPool(device, emptyBindlessPool, nullptr);
            vkDestroyPipelineLayout(device, defaultPipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, bindlessSetLayout, nullptr);
            m_emptyBindlessPool = nullptr;
            m_emptyBindlessSet = nullptr;
            m_defaultPipelineLayout = nullptr;
            m_bindlessSetLayout = nullptr;
            vkDestroyPipelineCache(device, pipelineCache, nullptr);
            m_pipelineCache = nullptr;
            vkDestroySampler(device, defaultSampler, nullptr);
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            m_descriptorPool = nullptr;
            m_defaultSampler = nullptr;
            for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
            AllocatorVK::Destroy(&m_allocatorVK);
            vkDestroyDevice(device, nullptr);
            InstanceVK::Destroy(&m_instanceVK);
            m_device = nullptr;
            PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
            return false;
        }
        m_frameSetLayout = frameLayout;
        m_forwardPassSetLayout = forwardPassLayout;
        m_postPassSetLayout = postPassLayout;
        m_shadowPassSetLayout = shadowPassLayout;
        m_lightCullPassSetLayout = lightCullPassLayout;
        m_deferredLightingPassSetLayout = deferredLightingPassLayout;
        m_materialSetLayout = materialLayout;
        m_sceneBuffersSetLayout = sceneLayout;
        m_iblPassSetLayout = iblPassLayout;
        m_emptyResourceSetLayout = emptyLayout;

        VkPipelineLayout forwardPl = vkres::CreateForwardPipelineLayout(device, frameLayout, forwardPassLayout, materialLayout, bindlessSetLayout,
                                                                        sceneLayout, m_supportsDescriptorBuffer);
        VkPipelineLayout postPl = vkres::CreatePostPipelineLayout(device, frameLayout, postPassLayout);
        VkPipelineLayout shadowPl = vkres::CreateShadowPipelineLayout(device, frameLayout, shadowPassLayout, sceneLayout);
        VkPipelineLayout gbufferPl = vkres::CreateGBufferPipelineLayout(device, frameLayout, materialLayout, bindlessSetLayout, sceneLayout,
                                                                      m_supportsDescriptorBuffer);
        VkPipelineLayout deferredLightingPl = vkres::CreateDeferredLightingPipelineLayout(device, frameLayout, deferredLightingPassLayout);
        VkPipelineLayout computeLightCullPl = vkres::CreateComputeLightCullPipelineLayout(device, frameLayout, lightCullPassLayout, sceneLayout, emptyLayout);
        VkPipelineLayout computeGpuCullPl = vkres::CreateComputeGpuCullPipelineLayout(device, frameLayout, sceneLayout, emptyLayout);
        VkPipelineLayout iblPl = vkres::CreateIBLPipelineLayout(device, iblPassLayout);
        VkPipelineLayout outlinePostPl = vkres::CreateOutlinePostPipelineLayout(device, outlinePostLayout);
        if (!forwardPl || !postPl || !shadowPl || !gbufferPl || !deferredLightingPl || !computeLightCullPl || !computeGpuCullPl || !iblPl || !outlinePostPl) {
            if (outlinePostPl) vkDestroyPipelineLayout(device, outlinePostPl, nullptr);
            if (iblPl) vkDestroyPipelineLayout(device, iblPl, nullptr);
            if (computeGpuCullPl) vkDestroyPipelineLayout(device, computeGpuCullPl, nullptr);
            if (computeLightCullPl) vkDestroyPipelineLayout(device, computeLightCullPl, nullptr);
            if (deferredLightingPl) vkDestroyPipelineLayout(device, deferredLightingPl, nullptr);
            if (gbufferPl) vkDestroyPipelineLayout(device, gbufferPl, nullptr);
            if (shadowPl) vkDestroyPipelineLayout(device, shadowPl, nullptr);
            if (postPl) vkDestroyPipelineLayout(device, postPl, nullptr);
            if (forwardPl) vkDestroyPipelineLayout(device, forwardPl, nullptr);
            vkDestroyDescriptorSetLayout(device, emptyLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, outlinePostLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, iblPassLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, sceneLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, deferredLightingPassLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, lightCullPassLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, shadowPassLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, postPassLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, forwardPassLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, frameLayout, nullptr);
            m_iblPassSetLayout = nullptr;
            m_frameSetLayout = nullptr;
            m_forwardPassSetLayout = nullptr;
            m_postPassSetLayout = nullptr;
            m_shadowPassSetLayout = nullptr;
            m_lightCullPassSetLayout = nullptr;
            m_deferredLightingPassSetLayout = nullptr;
            m_materialSetLayout = nullptr;
            m_sceneBuffersSetLayout = nullptr;
            core::Log::Error("DeviceVK::Initialize: resource-space pipeline layout creation failed");
            vkDestroyDescriptorPool(device, emptyBindlessPool, nullptr);
            vkDestroyPipelineLayout(device, defaultPipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, bindlessSetLayout, nullptr);
            m_emptyBindlessPool = nullptr;
            m_emptyBindlessSet = nullptr;
            m_defaultPipelineLayout = nullptr;
            m_bindlessSetLayout = nullptr;
            vkDestroyPipelineCache(device, pipelineCache, nullptr);
            m_pipelineCache = nullptr;
            vkDestroySampler(device, defaultSampler, nullptr);
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            m_descriptorPool = nullptr;
            m_defaultSampler = nullptr;
            for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
            AllocatorVK::Destroy(&m_allocatorVK);
            vkDestroyDevice(device, nullptr);
            InstanceVK::Destroy(&m_instanceVK);
            m_device = nullptr;
            PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
            return false;
        }
        m_forwardPipelineLayout = forwardPl;
        m_postPipelineLayout = postPl;
        m_shadowPipelineLayout = shadowPl;
        m_gbufferPipelineLayout = gbufferPl;
        m_deferredLightingPipelineLayout = deferredLightingPl;
        m_computeLightCullPipelineLayout = computeLightCullPl;
        m_computeGpuCullPipelineLayout = computeGpuCullPl;
        m_iblPipelineLayout = iblPl;
        m_outlinePostSetLayout = outlinePostLayout;
        m_outlinePostPipelineLayout = outlinePostPl;

        m_resourceSetLayouts[static_cast<size_t>(ResourceSetType::Frame)] =
            CreatePipelineLayoutWrapperVK(this, frameLayout, MakeResourceSetDesc(IDevice::ResourceSetType::Frame));
        m_resourceSetLayouts[static_cast<size_t>(ResourceSetType::PostPass)] =
            CreatePipelineLayoutWrapperVK(this, postPassLayout, MakeResourceSetDesc(IDevice::ResourceSetType::PostPass));
        m_resourceSetLayouts[static_cast<size_t>(ResourceSetType::ForwardPass)] =
            CreatePipelineLayoutWrapperVK(this, forwardPassLayout, MakeResourceSetDesc(IDevice::ResourceSetType::ForwardPass));
        m_resourceSetLayouts[static_cast<size_t>(ResourceSetType::ShadowPass)] =
            CreatePipelineLayoutWrapperVK(this, shadowPassLayout, MakeResourceSetDesc(IDevice::ResourceSetType::ShadowPass));
        m_resourceSetLayouts[static_cast<size_t>(ResourceSetType::LightCullPass)] =
            CreatePipelineLayoutWrapperVK(this, lightCullPassLayout, MakeResourceSetDesc(IDevice::ResourceSetType::LightCullPass));
        m_resourceSetLayouts[static_cast<size_t>(ResourceSetType::Material)] =
            CreatePipelineLayoutWrapperVK(this, materialLayout, MakeResourceSetDesc(IDevice::ResourceSetType::Material));
        m_resourceSetLayouts[static_cast<size_t>(ResourceSetType::SceneBuffers)] =
            CreatePipelineLayoutWrapperVK(this, sceneLayout, MakeResourceSetDesc(IDevice::ResourceSetType::SceneBuffers));
        m_resourceSetLayouts[static_cast<size_t>(ResourceSetType::DeferredLightingPass)] =
            CreatePipelineLayoutWrapperVK(this, deferredLightingPassLayout, MakeResourceSetDesc(IDevice::ResourceSetType::DeferredLightingPass));
        m_resourceSetLayouts[static_cast<size_t>(ResourceSetType::IBLPass)] =
            CreatePipelineLayoutWrapperVK(this, iblPassLayout, MakeResourceSetDesc(IDevice::ResourceSetType::IBLPass));
        m_resourceSetLayouts[static_cast<size_t>(ResourceSetType::OutlinePost)] =
            CreatePipelineLayoutWrapperVK(this, outlinePostLayout, MakeResourceSetDesc(IDevice::ResourceSetType::OutlinePost));
        m_emptyResourceSetLayoutWrapper = CreatePipelineLayoutWrapperVK(this, emptyLayout);
        m_emptyResourceSet = CreateDescriptorSet(m_emptyResourceSetLayoutWrapper);
    }

    VkSemaphoreTypeCreateInfo timelineTypeInfo = {};
    timelineTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineTypeInfo.initialValue = 0;
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = &timelineTypeInfo;
    VkSemaphore timelineSemaphore = VK_NULL_HANDLE;
    res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &timelineSemaphore);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateSemaphore (timeline) failed (0x%x)", res);
        vkDestroyDescriptorPool(device, emptyBindlessPool, nullptr);
        vkDestroyPipelineLayout(device, defaultPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, bindlessSetLayout, nullptr);
        m_emptyBindlessPool = nullptr;
        m_emptyBindlessSet = nullptr;
        m_defaultPipelineLayout = nullptr;
        m_bindlessSetLayout = nullptr;
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
        vkDestroySampler(device, defaultSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_defaultSampler = nullptr;
        m_descriptorPool = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_timelineSemaphore = timelineSemaphore;

    constexpr size_t kUploadHeapSize = 4 * 1024 * 1024;
    if (!UploadHeapVK::Create(&m_uploadHeap, GetAllocator(), kUploadHeapSize)) {
        core::Log::Error("DeviceVK::Initialize: UploadHeapVK::Create failed");
        vkDestroySemaphore(device, timelineSemaphore, nullptr);
        m_timelineSemaphore = nullptr;
        vkDestroyDescriptorPool(device, static_cast<VkDescriptorPool>(m_emptyBindlessPool), nullptr);
        vkDestroyPipelineLayout(device, static_cast<VkPipelineLayout>(m_defaultPipelineLayout), nullptr);
        vkDestroyDescriptorSetLayout(device, static_cast<VkDescriptorSetLayout>(m_bindlessSetLayout), nullptr);
        m_emptyBindlessPool = nullptr;
        m_emptyBindlessSet = nullptr;
        m_defaultPipelineLayout = nullptr;
        m_bindlessSetLayout = nullptr;
        vkDestroyPipelineCache(device, static_cast<VkPipelineCache>(m_pipelineCache), nullptr);
        m_pipelineCache = nullptr;
        vkDestroySampler(device, static_cast<VkSampler>(m_defaultSampler), nullptr);
        vkDestroyDescriptorPool(device, static_cast<VkDescriptorPool>(m_descriptorPool), nullptr);
        m_defaultSampler = nullptr;
        m_descriptorPool = nullptr;
        for (uint32_t i = 0; i < kMaxFrames; ++i) FrameContextVK::Destroy(&m_frameContexts[i], device);
        AllocatorVK::Destroy(&m_allocatorVK);
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }

    if (m_surface != nullptr) {
        if (!SwapchainVK::Create(&m_swapchain, device, m_physicalDeviceVK.GetHandle(), m_surface, width, height, this, m_swapchainPresentMode,
                                 m_supportsSwapchainMaintenance1)) {
            core::Log::ErrorF("DeviceVK::Initialize: SwapchainVK::Create failed");
        }
    }

    if (!CreateCrossQueueBridgeResources()) {
        core::Log::Warn("Vulkan: cross-queue buffer ownership bridge (graphics↔compute) could not be initialized; deferred compute lists with copies/barriers may fall back to the graphics queue.");
    }

    // Integer textures (e.g. entity ID R32UI) cannot be linearly filtered.
    VkSamplerCreateInfo pointSamplerInfo = {};
    pointSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    pointSamplerInfo.magFilter = VK_FILTER_NEAREST;
    pointSamplerInfo.minFilter = VK_FILTER_NEAREST;
    pointSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    pointSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    pointSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    pointSamplerInfo.anisotropyEnable = VK_FALSE;
    pointSamplerInfo.maxAnisotropy = 1.0f;
    pointSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    pointSamplerInfo.unnormalizedCoordinates = VK_FALSE;
    VkSampler nearestSamplerCreated = VK_NULL_HANDLE;
    if (vkCreateSampler(device, &pointSamplerInfo, nullptr, &nearestSamplerCreated) == VK_SUCCESS)
        m_nearestSampler = nearestSamplerCreated;
    else
        core::Log::Warn("DeviceVK::Initialize: failed to create nearest sampler, falling back to default linear sampler for all textures.");

    QueryCapabilities();
    core::Log::Info("Vulkan device initialized");
    return true;
}

void DeviceVK::SetSwapchainPresentMode(SwapchainPresentMode mode) {
    if (m_swapchainPresentMode == mode) return;
    m_swapchainPresentMode = mode;
    if (m_device && m_surface && m_swapchain.swapchain)
        m_swapchain.Resize(m_swapchain.GetWidth(), m_swapchain.GetHeight(), true, this, m_swapchainPresentMode);
}

void DeviceVK::SetSwapchainOutputMode(SwapchainOutputMode mode) {
    if (m_swapchainOutputMode == mode) return;
    m_swapchainOutputMode = mode;
    if (m_device && m_surface && m_swapchain.swapchain)
        m_swapchain.Resize(m_swapchain.GetWidth(), m_swapchain.GetHeight(), true, this, m_swapchainPresentMode);
}

void DeviceVK::GetSwapchainSurfaceColorInfo(uint32_t* outVkFormat, uint32_t* outVkColorSpaceKHR) const {
    if (outVkFormat) *outVkFormat = m_swapchain.GetFormat();
    if (outVkColorSpaceKHR) *outVkColorSpaceKHR = m_swapchain.GetColorSpace();
}

bool DeviceVK::WaitForPresent(uint64_t presentId, uint64_t timeoutNs) {
    if (!m_device || !m_supportsPresentWait || !m_pfnWaitForPresentKHR || !m_swapchain.swapchain)
        return false;
    if (presentId == 0)
        return false;
    auto* pfn = reinterpret_cast<PFN_vkWaitForPresentKHR>(m_pfnWaitForPresentKHR);
    VkResult r = pfn(static_cast<VkDevice>(m_device), static_cast<VkSwapchainKHR>(m_swapchain.swapchain), presentId, timeoutNs);
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        core::Log::WarnF("Vulkan WaitForPresent: vkWaitForPresentKHR returned 0x%x", static_cast<unsigned>(r));
        return false;
    }
    return true;
}

bool DeviceVK::WaitForPreviousPresent(uint64_t timeoutNs) {
    if (!m_supportsPresentWait)
        return false;
    const uint64_t id = GetLastPresentId();
    if (id == 0)
        return true;
    return WaitForPresent(id, timeoutNs);
}

uint64_t DeviceVK::GetLastPresentId() const {
    return m_swapchain.GetLastPresentId();
}

void DeviceVK::QueryCapabilities() {
    m_caps = RHICapabilities{};
    m_caps.supportsResourceSpaceBindPath = true;
    m_caps.supportsCompute = true;
    m_caps.supportsMultiDrawIndirect = true;
    m_caps.supportsMultiDrawIndirectCount = m_supportsDrawIndirectCount;
    m_caps.supportsGpuDrivenPerDrawTransformIndirect = true;
    m_caps.supportsAsyncCompute = (m_queues.GetComputeQueue() != nullptr) && (m_queues.GetComputeQueueFamily() != m_queues.GetGraphicsQueueFamily());
    m_caps.supportsBindless = true;  // descriptor indexing
    m_caps.supportsRayTracing = false;
    m_caps.supportsTessellation = true;
    m_caps.supportsGeometryShader = true;
    m_caps.supportsConditionalRender = false;
    m_caps.supportsTransformFeedback = false;
    m_caps.supportsSparseTexture = false;
    m_caps.supportsSparseBuffer = false;
    m_caps.supportsAtomicCounters = false;
    m_caps.supportsShaderSubroutines = false;

    m_caps.supportsPresentMailbox = false;
    m_caps.supportsPresentImmediate = false;
    m_caps.supportsGpuTimestampQueries = false;
    VkPhysicalDevice phys = static_cast<VkPhysicalDevice>(m_physicalDeviceVK.GetHandle());
    if (phys != VK_NULL_HANDLE) {
        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties(phys, &props);
        m_caps.supportsGpuTimestampQueries = props.limits.timestampPeriod > 0.f;
    }
    VkSurfaceKHR surface = static_cast<VkSurfaceKHR>(m_surface);
    if (surface != VK_NULL_HANDLE && phys != VK_NULL_HANDLE) {
        uint32_t modeCount = 0;
        if (vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, nullptr) == VK_SUCCESS && modeCount > 0) {
            std::vector<VkPresentModeKHR> modes(modeCount);
            if (vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, modes.data()) == VK_SUCCESS) {
                for (VkPresentModeKHR m : modes) {
                    if (m == VK_PRESENT_MODE_MAILBOX_KHR) m_caps.supportsPresentMailbox = true;
                    if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) m_caps.supportsPresentImmediate = true;
                }
            }
        }
    }
    m_caps.supportsOcclusionQueries = m_supportsOcclusionQuery;
    m_caps.supportsHostQueryReset = m_supportsHostQueryReset;
    m_caps.supportsPipelineStatisticsQueries = m_supportsPipelineStatisticsQuery;
    m_caps.supportsSynchronization2 = m_supportsSynchronization2;
    m_caps.supportsBufferDeviceAddress = m_supportsBufferDeviceAddress;
    m_caps.supportsDescriptorBuffer = m_supportsDescriptorBuffer;
    m_caps.supportsGraphicsPipelineLibrary = m_supportsGraphicsPipelineLibrary;
    m_caps.supportsPipelineExecutableProperties = m_supportsPipelineExecutableProperties;
    m_caps.supportsDeviceFault = m_supportsDeviceFault;
    m_caps.supportsSwapchainMaintenance1 = m_supportsSwapchainMaintenance1;
    m_caps.supportsSwapchainColorspace = m_supportsSwapchainColorspace;
    m_caps.supportsPresentWait = m_supportsPresentWait;
    core::Log::InfoF(
        "Vulkan caps: present mailbox=%d immediate=%d gpuTimestamps=%d tess=%d geom=%d occlusion=%d hostQueryReset=%d pipelineStats=%d sync2=%d bda=%d descriptorBuffer=%d graphicsPipelineLibrary=%d pipelineExecutableProperties=%d deviceFault=%d asyncCompute=%d swapchainMaint1=%d swapchainColorspace=%d presentWait=%d",
        m_caps.supportsPresentMailbox ? 1 : 0, m_caps.supportsPresentImmediate ? 1 : 0,
        m_caps.supportsGpuTimestampQueries ? 1 : 0, m_caps.supportsTessellation ? 1 : 0,
        m_caps.supportsGeometryShader ? 1 : 0, m_caps.supportsOcclusionQueries ? 1 : 0,
        m_caps.supportsHostQueryReset ? 1 : 0, m_caps.supportsPipelineStatisticsQueries ? 1 : 0,
        m_caps.supportsSynchronization2 ? 1 : 0, m_caps.supportsBufferDeviceAddress ? 1 : 0, m_caps.supportsDescriptorBuffer ? 1 : 0,
        m_caps.supportsGraphicsPipelineLibrary ? 1 : 0,
        m_caps.supportsPipelineExecutableProperties ? 1 : 0, m_caps.supportsDeviceFault ? 1 : 0,
        m_caps.supportsAsyncCompute ? 1 : 0, m_caps.supportsSwapchainMaintenance1 ? 1 : 0,
        m_caps.supportsSwapchainColorspace ? 1 : 0, m_caps.supportsPresentWait ? 1 : 0);
}

void DeviceVK::EnsureFullscreenQuad() {
    if (m_fullscreenQuadVBO && m_fullscreenQuadIBO) return;
    const float vertices[] = {
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,
    };
    const uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
    BufferDesc vboDesc;
    vboDesc.sizeInBytes = sizeof(vertices);
    vboDesc.usage = BufferUsage::Vertex;
    m_fullscreenQuadVBO = CreateBuffer(vboDesc, vertices);
    BufferDesc iboDesc;
    iboDesc.sizeInBytes = sizeof(indices);
    iboDesc.usage = BufferUsage::Index;
    m_fullscreenQuadIBO = CreateBuffer(iboDesc, indices);
}

void DeviceVK::UpdateComputeSet0(CommandListVK* cmd) {
    vkhelpers::UpdateComputeSet0(this, cmd);
}

void* DeviceVK::GetPipelineLayoutForKind(PipelineLayoutKind kind) const {
    return vkres::GetPipelineLayoutForKind(
        static_cast<VkPipelineLayout>(m_forwardPipelineLayout),
        static_cast<VkPipelineLayout>(m_postPipelineLayout),
        static_cast<VkPipelineLayout>(m_shadowPipelineLayout),
        static_cast<VkPipelineLayout>(m_gbufferPipelineLayout),
        static_cast<VkPipelineLayout>(m_deferredLightingPipelineLayout),
        static_cast<VkPipelineLayout>(m_computeLightCullPipelineLayout),
        static_cast<VkPipelineLayout>(m_computeGpuCullPipelineLayout),
        static_cast<VkPipelineLayout>(m_iblPipelineLayout),
        static_cast<VkPipelineLayout>(m_outlinePostPipelineLayout),
        static_cast<VkPipelineLayout>(m_defaultPipelineLayout),
        kind);
}

const RHICapabilities& DeviceVK::GetCapabilities() const { return m_caps; }

const char* DeviceVK::GetShaderBackendName() const { return "vulkan"; }

const char* DeviceVK::GetShaderVertexExtension() const { return ".vert"; }

const char* DeviceVK::GetShaderFragmentExtension() const { return ".frag"; }

bool DeviceVK::ResolveShaderPath(const char* logicalName, ShaderStage stage, std::string& outPath) {
    if (!logicalName || logicalName[0] == '\0') return false;
    const char* ext = (stage == ShaderStage::Compute) ? ".comp" : (stage == ShaderStage::Vertex) ? ".vert" : ".frag";
    outPath = std::string("assets/shaders/vulkan/") + logicalName + ext;
    return true;
}

void* DeviceVK::GetCachedGraphicsPipeline(uint64_t hash) const {
    auto it = m_graphicsPipelineCache.find(hash);
    return (it != m_graphicsPipelineCache.end()) ? it->second.vkPipeline : nullptr;
}

void DeviceVK::RegisterGraphicsPipeline(uint64_t hash, void* pipeline) {
    if (!pipeline) return;
    m_graphicsPipelineCache[hash] = GraphicsPipelineCacheEntry{pipeline, 0};
}

void DeviceVK::RetainGraphicsPipeline(uint64_t hash) {
    auto it = m_graphicsPipelineCache.find(hash);
    if (it == m_graphicsPipelineCache.end() || !it->second.vkPipeline) {
        core::Log::Error("RetainGraphicsPipeline: hash not registered (internal error).");
        return;
    }
    ++it->second.refCount;
}

void DeviceVK::ReleaseGraphicsPipeline(uint64_t hash, void* vkPipeline) {
    if (!vkPipeline) return;
    auto it = m_graphicsPipelineCache.find(hash);
    if (it == m_graphicsPipelineCache.end()) {
        // Cache already cleared (e.g. device teardown) or unknown hash — do not vkDestroy (may already be freed).
        core::Log::Warn("ReleaseGraphicsPipeline: hash not in cache (skip release).");
        return;
    }
    if (it->second.vkPipeline != vkPipeline) {
        core::Log::Warn("ReleaseGraphicsPipeline: handle mismatch (skip release).");
        return;
    }
    if (it->second.refCount == 0) {
        core::Log::Warn("ReleaseGraphicsPipeline: refcount underflow (internal error).");
        return;
    }
    --it->second.refCount;
    if (it->second.refCount == 0) {
        m_deletionQueue.EnqueuePipeline(GetCurrentFrameIndex(), m_device, it->second.vkPipeline);
        m_graphicsPipelineCache.erase(it);
    }
}

void DeviceVK::FreeCommandBuffer(void* cmdBuf, void* commandPool) {
    if (!cmdBuf || !commandPool || !m_device) return;
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(cmdBuf);
    vkFreeCommandBuffers(static_cast<VkDevice>(m_device), static_cast<VkCommandPool>(commandPool), 1, &buf);
}

ICommandList* DeviceVK::CreateCommandList() {
    if (!m_device || !m_queues.GetGraphicsQueue()) return nullptr;
    FrameContextVK& frame = m_frameContexts[m_currentFrameIndex];
    VkCommandPool pool = static_cast<VkCommandPool>(frame.commandPool);
    if (pool == VK_NULL_HANDLE) return nullptr;
    VkCommandBuffer buf = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkResult res = vkAllocateCommandBuffers(static_cast<VkDevice>(m_device), &allocInfo, &buf);
    if (res != VK_SUCCESS) return nullptr;
    return new CommandListVK(this, buf, frame.commandPool);
}

void DeviceVK::DestroyCommandList(ICommandList* list) {
    if (!list) return;
    CommandListVK* cl = static_cast<CommandListVK*>(list);
    delete cl;  // dtor calls FreeCommandBuffer(buffer, pool) for deferred lists
}

ICommandList* DeviceVK::GetImmediateCommandList() { return m_immediateCommandList.get(); }

ICommandList* DeviceVK::GetTransientRecordingCommandList() const {
    return m_transientRecordingStack.empty() ? nullptr : m_transientRecordingStack.back();
}

void DeviceVK::PushTransientRecordingCommandList(ICommandList* cmd) {
    if (cmd)
        m_transientRecordingStack.push_back(cmd);
}

void DeviceVK::PopTransientRecordingCommandList(ICommandList* cmd) {
    assert(!m_transientRecordingStack.empty() && m_transientRecordingStack.back() == cmd &&
           "PopTransientRecordingCommandList: mismatched or empty stack");
    if (!m_transientRecordingStack.empty() && m_transientRecordingStack.back() == cmd)
        m_transientRecordingStack.pop_back();
}

void DeviceVK::EnqueueFrameSliceCommandList(ICommandList* cmd) {
    if (!cmd) return;
    m_frameSliceCommandListsByFrame[m_currentFrameIndex].push_back(std::unique_ptr<ICommandList>(cmd));
}

void DeviceVK::NoteVulkanDeviceLost(const char* context, int vkResult) {
#if defined(VK_ERROR_DEVICE_LOST)
    if (static_cast<VkResult>(vkResult) != VK_ERROR_DEVICE_LOST)
        return;
#else
    if (vkResult != -4) // VK_ERROR_DEVICE_LOST
        return;
#endif
    if (m_deviceLost)
        return;
    m_deviceLost = true;
    core::Log::ErrorF(
        "Vulkan: VK_ERROR_DEVICE_LOST (%s) — `VkDevice` and dependent RHI objects are invalid; destroy this device and "
        "rebuild swapchain, pipelines, and GPU resources from a fresh init (see `documentation/vulkan/steps_todo.md` Phase E).",
        context ? context : "?");
}

CommandListVK* DeviceVK::GetRecordingCommandListVK() {
    if (!m_transientRecordingStack.empty())
        return static_cast<CommandListVK*>(m_transientRecordingStack.back());
    return m_immediateCommandList.get();
}

void DeviceVK::BeginFrame() {
    assert(m_gpuFramePhase == GpuFramePhase::BetweenFrames && "BeginFrame: phase must be BetweenFrames");
    if (!m_immediateCommandList || !m_device || !m_queues.GetGraphicsQueue()) return;
    m_currentFrameIndex = (m_currentFrameIndex + 1) % kMaxFrames;
    ++m_frameNumber;
    FrameContextVK& frame = m_frameContexts[m_currentFrameIndex];
    VkDevice device = static_cast<VkDevice>(m_device);
    VkFence fence = static_cast<VkFence>(frame.fence);
    VkCommandPool pool = static_cast<VkCommandPool>(frame.commandPool);
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(frame.commandBuffer.GetHandle());
    if (fence == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || buf == VK_NULL_HANDLE) return;

    const VkResult fenceWait = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    NoteVulkanDeviceLost("BeginFrame vkWaitForFences", static_cast<int>(fenceWait));
    // Free slice CBs from this ring slot only: same fence waited above covers their last submit.
    m_frameSliceCommandListsByFrame[m_currentFrameIndex].clear();
    m_transientRecordingStack.clear();
    m_deletionQueue.Tick(m_device, GetAllocator(), m_currentFrameIndex);
    RetirePendingTextureWrappers(false);
    ++m_ticksUntilDescriptorSetFree;
    if (m_ticksUntilDescriptorSetFree >= kMaxFrames && !m_pendingDescriptorSetsToFree.empty()) {
        for (IDescriptorSet* set : m_pendingDescriptorSetsToFree)
            DestroyDescriptorSetVK(this, set);
        m_pendingDescriptorSetsToFree.clear();
        m_ticksUntilDescriptorSetFree = 0;
    }
    UploadHeapVK::BeginFrame(&m_uploadHeap, m_device, frame.fence, m_currentFrameIndex);
    DescriptorAllocatorVK::Reset(&m_descriptorAllocator, m_device, m_currentFrameIndex);
    DescriptorCacheVK::Reset(&m_descriptorCache, m_device, m_currentFrameIndex);
    BarrierTrackerVK::Reset(&m_barrierTracker);
    vkResetFences(device, 1, &fence);
    vkResetCommandPool(device, pool, 0);

    // Compose buffer is vkBegin'd lazily (after optional editor slice command buffers) so Scene/Game stay isolated.
    m_immediateCommandList->SetCommandBuffer(buf, true);
    m_immediateCommandList->ResetCpuRecordingStateAfterPoolReset();
    m_immediateCommandList->SetRecordingState(false);
    m_frameOpen = true;
    m_gpuFramePhase = GpuFramePhase::Recording;

    // Acquire swapchain image after command buffer is ready (allows recording while waiting in more complex setups).
    if (m_swapchain.swapchain && m_swapchain.GetImageCount() > 0) {
        uint32_t acquiredIndex = 0xFFFFFFFFu;
        int acquireRes = m_swapchain.Acquire(frame.imageAvailable, UINT64_MAX, &acquiredIndex);
        NoteVulkanDeviceLost("BeginFrame vkAcquireNextImageKHR", acquireRes);
        if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
            m_swapchain.Resize(m_swapchain.GetWidth(), m_swapchain.GetHeight(), true, this, m_swapchainPresentMode);
            m_pendingPresent = false;
            frame.swapchainImageIndex = 0xFFFFFFFFu;
        } else if (acquireRes == VK_SUCCESS || acquireRes == VK_SUBOPTIMAL_KHR) {
            m_pendingPresent = true;
            frame.swapchainImageIndex = acquiredIndex;
            if (void* acqImg = m_swapchain.GetImage(acquiredIndex))
                BarrierTrackerVK::SetImageState(&m_barrierTracker, acqImg, ResourceState::Undefined);
        } else {
            m_pendingPresent = false;
            frame.swapchainImageIndex = 0xFFFFFFFFu;
            static int s_acquireFailLog = 0;
            if (s_acquireFailLog < 6) {
                ++s_acquireFailLog;
                core::Log::WarnF("Vulkan BeginFrame: vkAcquireNextImageKHR failed (0x%x)", static_cast<unsigned>(acquireRes));
            }
        }
    } else {
        m_pendingPresent = false;
        frame.swapchainImageIndex = 0xFFFFFFFFu;
    }
}

// Submission is device-owned: only EndFrame() / SubmitCommandList() submit to the queue. Renderer only records.
void DeviceVK::EndFrame() {
    m_frameOpen = false;
    assert(m_gpuFramePhase == GpuFramePhase::Recording && "EndFrame: expected Recording phase");
    m_gpuFramePhase = GpuFramePhase::Submitting;
    if (!m_immediateCommandList || !m_queues.GetGraphicsQueue()) {
        m_gpuFramePhase = GpuFramePhase::BetweenFrames;
        static bool once = false;
        if (!once) {
            core::Log::Warn("Vulkan EndFrame: skipped — no immediate command list or no graphics queue.");
            once = true;
        }
        return;
    }
    CommandListVK* imm = m_immediateCommandList.get();
    std::vector<VkCommandBuffer> submitBuffers;
    std::vector<std::unique_ptr<ICommandList>>& sliceBucket = m_frameSliceCommandListsByFrame[m_currentFrameIndex];
    submitBuffers.reserve(sliceBucket.size() + 1u);
    for (auto& up : sliceBucket) {
        if (!up) continue;
        CommandListVK* sliceCl = static_cast<CommandListVK*>(up.get());
        VkCommandBuffer sliceBuf = static_cast<VkCommandBuffer>(sliceCl->GetCommandBuffer());
        if (sliceBuf != VK_NULL_HANDLE)
            submitBuffers.push_back(sliceBuf);
    }
    if (!imm->IsRecording())
        imm->Begin();
    imm->End();
    VkCommandBuffer immBuf = static_cast<VkCommandBuffer>(imm->GetCommandBuffer());
    if (immBuf != VK_NULL_HANDLE)
        submitBuffers.push_back(immBuf);
    VkQueue queue = static_cast<VkQueue>(m_queues.GetGraphicsQueue());
    FrameContextVK& frame = m_frameContexts[m_currentFrameIndex];
    VkFence fence = static_cast<VkFence>(frame.fence);
    if (submitBuffers.empty() || fence == VK_NULL_HANDLE) {
        m_gpuFramePhase = GpuFramePhase::BetweenFrames;
        static bool once = false;
        if (!once) {
            core::Log::Warn("Vulkan EndFrame: submit skipped — no command buffers or null fence.");
            once = true;
        }
        return;
    }
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = static_cast<uint32_t>(submitBuffers.size());
    submitInfo.pCommandBuffers = submitBuffers.data();
    VkSemaphore waitSem = static_cast<VkSemaphore>(frame.imageAvailable);
    const uint32_t imageIndex = frame.swapchainImageIndex;
    void* renderFinishedPerImage = m_swapchain.GetRenderFinishedSemaphore(imageIndex);
    VkSemaphore signalSem = renderFinishedPerImage ? static_cast<VkSemaphore>(renderFinishedPerImage) : static_cast<VkSemaphore>(frame.renderFinished);
    if (m_pendingPresent && waitSem != VK_NULL_HANDLE && signalSem != VK_NULL_HANDLE) {
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitSem;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSem;
    } else if (m_pendingPresent) {
        static bool once = false;
        if (!once) {
            core::Log::Warn("Vulkan EndFrame: m_pendingPresent true but semaphores null — present may not wait for image.");
            once = true;
        }
    }
    VkResult submitRes = vkQueueSubmit(queue, 1, &submitInfo, fence);
    if (submitRes != VK_SUCCESS)
        core::Log::ErrorF("Vulkan EndFrame: vkQueueSubmit failed (0x%x)", submitRes);
    NoteVulkanDeviceLost("EndFrame vkQueueSubmit", static_cast<int>(submitRes));
    m_gpuFramePhase = GpuFramePhase::BetweenFrames;
}

void DeviceVK::ReleaseSwapchainImagesForMaintenance(void* vkSwapchain, uint32_t imageCount) {
#if defined(VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT)
    if (!m_supportsSwapchainMaintenance1 || !m_pfnReleaseSwapchainImagesEXT || !vkSwapchain || imageCount == 0)
        return;
    VkDevice dev = static_cast<VkDevice>(m_device);
    if (dev == VK_NULL_HANDLE)
        return;
    std::vector<uint32_t> indices(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
        indices[i] = i;
    VkReleaseSwapchainImagesInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT;
    info.swapchain = static_cast<VkSwapchainKHR>(vkSwapchain);
    info.imageIndexCount = imageCount;
    info.pImageIndices = indices.data();
    auto pfn = reinterpret_cast<PFN_vkReleaseSwapchainImagesEXT>(m_pfnReleaseSwapchainImagesEXT);
    VkResult r = pfn(dev, &info);
    if (r != VK_SUCCESS)
        core::Log::WarnF("Vulkan: vkReleaseSwapchainImagesEXT failed (0x%x) before swapchain destroy.", static_cast<unsigned>(r));
#else
    (void)vkSwapchain;
    (void)imageCount;
#endif
}

void DeviceVK::DestroyCrossQueueBridge() {
    VkDevice device = static_cast<VkDevice>(m_device);
    if (device != VK_NULL_HANDLE) {
        if (m_crossQueueReleasePool) {
            vkDestroyCommandPool(device, static_cast<VkCommandPool>(m_crossQueueReleasePool), nullptr);
            m_crossQueueReleasePool = nullptr;
            m_crossQueueReleaseCmd = nullptr;
        }
        if (m_crossQueueAcquirePool) {
            vkDestroyCommandPool(device, static_cast<VkCommandPool>(m_crossQueueAcquirePool), nullptr);
            m_crossQueueAcquirePool = nullptr;
            m_crossQueueAcquireCmd = nullptr;
        }
    } else {
        m_crossQueueReleasePool = nullptr;
        m_crossQueueReleaseCmd = nullptr;
        m_crossQueueAcquirePool = nullptr;
        m_crossQueueAcquireCmd = nullptr;
    }
    m_crossQueueBridgeReady = false;
}

bool DeviceVK::CreateCrossQueueBridgeResources() {
    DestroyCrossQueueBridge();
    VkDevice device = static_cast<VkDevice>(m_device);
    if (device == VK_NULL_HANDLE)
        return false;
    const uint32_t gfxFam = m_queues.GetGraphicsQueueFamily();
    const uint32_t cmpFam = m_queues.GetComputeQueueFamily();
    if (gfxFam == cmpFam)
        return true;

    VkCommandPool releasePool = VK_NULL_HANDLE;
    VkCommandPool acquirePool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo pci = {};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pci.queueFamilyIndex = gfxFam;
    if (vkCreateCommandPool(device, &pci, nullptr, &releasePool) != VK_SUCCESS)
        return false;
    pci.queueFamilyIndex = cmpFam;
    if (vkCreateCommandPool(device, &pci, nullptr, &acquirePool) != VK_SUCCESS) {
        vkDestroyCommandPool(device, releasePool, nullptr);
        return false;
    }
    VkCommandBufferAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = releasePool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer releaseCmd = VK_NULL_HANDLE;
    VkCommandBuffer acquireCmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &ai, &releaseCmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device, acquirePool, nullptr);
        vkDestroyCommandPool(device, releasePool, nullptr);
        return false;
    }
    ai.commandPool = acquirePool;
    if (vkAllocateCommandBuffers(device, &ai, &acquireCmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device, acquirePool, nullptr);
        vkDestroyCommandPool(device, releasePool, nullptr);
        return false;
    }
    m_crossQueueReleasePool = releasePool;
    m_crossQueueReleaseCmd = releaseCmd;
    m_crossQueueAcquirePool = acquirePool;
    m_crossQueueAcquireCmd = acquireCmd;
    m_crossQueueBridgeReady = true;
    return true;
}

void DeviceVK::SetAsyncComputeSubmitPolicy(AsyncComputeSubmitPolicy policy) {
    m_asyncComputeSubmitPolicy = policy;
}

AsyncComputeSubmitPolicy DeviceVK::GetAsyncComputeSubmitPolicy() const {
    return m_asyncComputeSubmitPolicy;
}

/*
 * Deferred SubmitCommandList — compute vs graphics queue (canonical eligibility: documentation/vulkan/steps_todo.md Phase C).
 *
 * CommandListVK::IsComputeQueueEligible() == (m_computeQueueEligible && m_hasComputeWork && !m_hasGraphicsWork).
 * Graphics work or clearing eligibility: BindPipeline(graphics), draws, render passes, clears, most blits/copies that mark graphics, etc.
 * m_computeQueueEligible cleared: BarrierTexture when tracker layout was Undefined; MemoryBarrierStorage; host/transfer barriers on
 * host-visible paths (BarrierBufferTransferReadToHostWrite, BarrierHostVisibleBufferToTransferRead); many graphics-only paths.
 * RequiresQueueOwnershipTransfers: BarrierBuffer, BarrierTexture, CopyBuffer, descriptor-buffer bind barrier path, etc.
 * CrossQueueOwnershipUnsafe: global memory barriers, host-visible buffer barriers, texture barrier from Undefined (cannot bridge safely).
 *
 * Submit routing:
 * - If policy is ForceGraphicsQueue: always graphics queue (timeline ordering unchanged).
 * - Else if useResourceBridge: graphics release submit (queue-family release) then compute submit (acquire + user CB), timeline-linked.
 * - Else canUseComputeQueue: dedicated compute family AND no ownership transfers required, OR same family; else graphics fallback + warn when eligible but unsafe.
 */
void DeviceVK::SubmitCommandList(ICommandList* list) {
    if (!list || !m_queues.GetGraphicsQueue()) return;
    CommandListVK* cl = static_cast<CommandListVK*>(list);
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(cl->GetCommandBuffer());
    if (buf == VK_NULL_HANDLE) return;
    bool wantsComputeQueue = cl->IsComputeQueueEligible();
    if (m_asyncComputeSubmitPolicy == AsyncComputeSubmitPolicy::ForceGraphicsQueue)
        wantsComputeQueue = false;
    if (wantsComputeQueue && !m_caps.supportsAsyncCompute) {
        core::Log::Warn("Vulkan SubmitCommandList: command list marked compute-queue-eligible, but async compute is unsupported; falling back to graphics queue.");
    }
    if (wantsComputeQueue && !m_queues.GetComputeQueue()) {
        core::Log::Warn("Vulkan SubmitCommandList: command list marked compute-queue-eligible, but compute queue is unavailable; falling back to graphics queue.");
    }
    const uint32_t gfxFam = m_queues.GetGraphicsQueueFamily();
    const uint32_t cmpFam = m_queues.GetComputeQueueFamily();
    const bool hasDedicatedComputeFamily = (cmpFam != gfxFam);
    const bool requiresOwnershipTransfers = cl->RequiresQueueOwnershipTransfers();
    const bool ownershipUnsafe = cl->CrossQueueOwnershipUnsafe();
    const std::vector<void*>& bridgeBuffers = cl->GetCrossQueueBuffers();
    const std::vector<CrossQueueImageMeta>& bridgeImages = cl->GetCrossQueueImages();
    const bool useResourceBridge = m_crossQueueBridgeReady && m_caps.supportsAsyncCompute && wantsComputeQueue && m_queues.GetComputeQueue() &&
                                   hasDedicatedComputeFamily && requiresOwnershipTransfers && !ownershipUnsafe &&
                                   (!bridgeBuffers.empty() || !bridgeImages.empty()) &&
                                   (static_cast<VkSemaphore>(m_timelineSemaphore) != VK_NULL_HANDLE);

    if (wantsComputeQueue && hasDedicatedComputeFamily && requiresOwnershipTransfers && !useResourceBridge) {
        core::Log::Warn(
            "Vulkan SubmitCommandList: queue-ownership-sensitive work cannot use the async compute queue (no buffer/image bridge or unsafe barriers); falling back to graphics queue.");
    }

    VkSemaphore timelineSem = static_cast<VkSemaphore>(m_timelineSemaphore);
    uint64_t signalValue = cl->GetPendingTimelineValue();
    if (signalValue == 0) {
        signalValue = ++m_timelineValue;
    } else if (signalValue > m_timelineValue) {
        m_timelineValue = signalValue;
    }
    const uint64_t waitValue = m_lastDeferredSubmitTimelineValue;
    const bool needTimelineSync = (timelineSem != VK_NULL_HANDLE);

    if (useResourceBridge) {
        uint64_t releaseValue = ++m_timelineValue;
        if (signalValue <= releaseValue) {
            signalValue = releaseValue + 1;
            m_timelineValue = signalValue;
        }
        const bool waitTimeline = needTimelineSync && (waitValue != 0);
        const bool signalTimeline = needTimelineSync && (releaseValue != 0);
        const bool signalFinalTimeline = needTimelineSync && (signalValue != 0);

        VkCommandBuffer releaseCmd = static_cast<VkCommandBuffer>(m_crossQueueReleaseCmd);
        VkCommandBuffer acquireCmd = static_cast<VkCommandBuffer>(m_crossQueueAcquireCmd);
        vkResetCommandBuffer(releaseCmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        VkCommandBufferBeginInfo beginRel = {};
        beginRel.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginRel.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(releaseCmd, &beginRel);
        std::vector<VkBufferMemoryBarrier2> relBarriers;
        relBarriers.reserve(bridgeBuffers.size());
        for (void* h : bridgeBuffers) {
            VkBuffer vkBuf = static_cast<VkBuffer>(h);
            if (vkBuf == VK_NULL_HANDLE)
                continue;
            VkBufferMemoryBarrier2 b = {};
            b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            b.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            b.dstAccessMask = 0;
            b.srcQueueFamilyIndex = gfxFam;
            b.dstQueueFamilyIndex = cmpFam;
            b.buffer = vkBuf;
            b.offset = 0;
            b.size = VK_WHOLE_SIZE;
            relBarriers.push_back(b);
        }
        std::vector<VkImageMemoryBarrier2> relImgBarriers;
        relImgBarriers.reserve(bridgeImages.size());
        for (const CrossQueueImageMeta& im : bridgeImages) {
            if (!im.vkImage)
                continue;
            const VkImageAspectFlags aspect = static_cast<VkImageAspectFlags>(im.aspectMask);
            const bool depthish = (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
            VkImageLayout layout = ToVkImageLayout(im.ownershipLayout);
            if (depthish && im.ownershipLayout == ResourceState::RenderTarget)
                layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            VkImageMemoryBarrier2 ib = {};
            ib.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            ib.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            ib.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            ib.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            ib.dstAccessMask = 0;
            ib.oldLayout = layout;
            ib.newLayout = layout;
            ib.srcQueueFamilyIndex = gfxFam;
            ib.dstQueueFamilyIndex = cmpFam;
            ib.image = static_cast<VkImage>(im.vkImage);
            ib.subresourceRange.aspectMask = aspect;
            ib.subresourceRange.baseMipLevel = im.baseMipLevel;
            ib.subresourceRange.levelCount = im.levelCount;
            ib.subresourceRange.baseArrayLayer = im.baseArrayLayer;
            ib.subresourceRange.layerCount = im.arrayLayerCount;
            relImgBarriers.push_back(ib);
        }
        if (!relBarriers.empty() || !relImgBarriers.empty()) {
            VkDependencyInfo depRel = {};
            depRel.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            depRel.bufferMemoryBarrierCount = static_cast<uint32_t>(relBarriers.size());
            depRel.pBufferMemoryBarriers = relBarriers.empty() ? nullptr : relBarriers.data();
            depRel.imageMemoryBarrierCount = static_cast<uint32_t>(relImgBarriers.size());
            depRel.pImageMemoryBarriers = relImgBarriers.empty() ? nullptr : relImgBarriers.data();
            vkCmdPipelineBarrier2(releaseCmd, &depRel);
        }
        vkEndCommandBuffer(releaseCmd);

        vkResetCommandBuffer(acquireCmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        VkCommandBufferBeginInfo beginAcq = {};
        beginAcq.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginAcq.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(acquireCmd, &beginAcq);
        std::vector<VkBufferMemoryBarrier2> acqBarriers;
        acqBarriers.reserve(bridgeBuffers.size());
        for (void* h : bridgeBuffers) {
            VkBuffer vkBuf = static_cast<VkBuffer>(h);
            if (vkBuf == VK_NULL_HANDLE)
                continue;
            VkBufferMemoryBarrier2 b = {};
            b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            b.srcAccessMask = 0;
            b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            b.srcQueueFamilyIndex = gfxFam;
            b.dstQueueFamilyIndex = cmpFam;
            b.buffer = vkBuf;
            b.offset = 0;
            b.size = VK_WHOLE_SIZE;
            acqBarriers.push_back(b);
        }
        std::vector<VkImageMemoryBarrier2> acqImgBarriers;
        acqImgBarriers.reserve(bridgeImages.size());
        for (const CrossQueueImageMeta& im : bridgeImages) {
            if (!im.vkImage)
                continue;
            const VkImageAspectFlags aspect = static_cast<VkImageAspectFlags>(im.aspectMask);
            const bool depthish = (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
            VkImageLayout layout = ToVkImageLayout(im.ownershipLayout);
            if (depthish && im.ownershipLayout == ResourceState::RenderTarget)
                layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            VkImageMemoryBarrier2 ib = {};
            ib.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            ib.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            ib.srcAccessMask = 0;
            ib.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            ib.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            ib.oldLayout = layout;
            ib.newLayout = layout;
            ib.srcQueueFamilyIndex = gfxFam;
            ib.dstQueueFamilyIndex = cmpFam;
            ib.image = static_cast<VkImage>(im.vkImage);
            ib.subresourceRange.aspectMask = aspect;
            ib.subresourceRange.baseMipLevel = im.baseMipLevel;
            ib.subresourceRange.levelCount = im.levelCount;
            ib.subresourceRange.baseArrayLayer = im.baseArrayLayer;
            ib.subresourceRange.layerCount = im.arrayLayerCount;
            acqImgBarriers.push_back(ib);
        }
        if (!acqBarriers.empty() || !acqImgBarriers.empty()) {
            VkDependencyInfo depAcq = {};
            depAcq.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            depAcq.bufferMemoryBarrierCount = static_cast<uint32_t>(acqBarriers.size());
            depAcq.pBufferMemoryBarriers = acqBarriers.empty() ? nullptr : acqBarriers.data();
            depAcq.imageMemoryBarrierCount = static_cast<uint32_t>(acqImgBarriers.size());
            depAcq.pImageMemoryBarriers = acqImgBarriers.empty() ? nullptr : acqImgBarriers.data();
            vkCmdPipelineBarrier2(acquireCmd, &depAcq);
        }
        vkEndCommandBuffer(acquireCmd);

        VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkTimelineSemaphoreSubmitInfo timelineInfoG = {};
        timelineInfoG.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfoG.waitSemaphoreValueCount = waitTimeline ? 1u : 0u;
        timelineInfoG.pWaitSemaphoreValues = waitTimeline ? &waitValue : nullptr;
        timelineInfoG.signalSemaphoreValueCount = signalTimeline ? 1u : 0u;
        timelineInfoG.pSignalSemaphoreValues = &releaseValue;
        VkSubmitInfo submitG = {};
        submitG.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitG.pNext = needTimelineSync ? &timelineInfoG : nullptr;
        submitG.commandBufferCount = 1;
        submitG.pCommandBuffers = &releaseCmd;
        submitG.waitSemaphoreCount = waitTimeline ? 1u : 0u;
        submitG.pWaitSemaphores = waitTimeline ? &timelineSem : nullptr;
        submitG.pWaitDstStageMask = waitTimeline ? &waitStageMask : nullptr;
        submitG.signalSemaphoreCount = signalTimeline ? 1u : 0u;
        submitG.pSignalSemaphores = signalTimeline ? &timelineSem : nullptr;
        VkQueue gfxQueue = static_cast<VkQueue>(m_queues.GetGraphicsQueue());
        VkResult gr = vkQueueSubmit(gfxQueue, 1, &submitG, VK_NULL_HANDLE);
        if (gr != VK_SUCCESS) {
            core::Log::ErrorF("Vulkan SubmitCommandList: graphics release submit failed (0x%x)", gr);
            NoteVulkanDeviceLost("SubmitCommandList vkQueueSubmit (graphics release)", static_cast<int>(gr));
            if (signalValue != 0)
                cl->ClearPendingTimelineValue();
            return;
        }

        VkCommandBuffer computeCmds[2] = { acquireCmd, buf };
        VkTimelineSemaphoreSubmitInfo timelineInfoC = {};
        timelineInfoC.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfoC.waitSemaphoreValueCount = signalTimeline ? 1u : 0u;
        timelineInfoC.pWaitSemaphoreValues = signalTimeline ? &releaseValue : nullptr;
        timelineInfoC.signalSemaphoreValueCount = signalFinalTimeline ? 1u : 0u;
        timelineInfoC.pSignalSemaphoreValues = &signalValue;
        VkSubmitInfo submitC = {};
        submitC.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitC.pNext = needTimelineSync ? &timelineInfoC : nullptr;
        submitC.commandBufferCount = 2;
        submitC.pCommandBuffers = computeCmds;
        submitC.waitSemaphoreCount = signalTimeline ? 1u : 0u;
        submitC.pWaitSemaphores = signalTimeline ? &timelineSem : nullptr;
        submitC.pWaitDstStageMask = signalTimeline ? &waitStageMask : nullptr;
        submitC.signalSemaphoreCount = signalFinalTimeline ? 1u : 0u;
        submitC.pSignalSemaphores = signalFinalTimeline ? &timelineSem : nullptr;
        VkQueue cmpQueue = static_cast<VkQueue>(m_queues.GetComputeQueue());
        VkResult cr = vkQueueSubmit(cmpQueue, 1, &submitC, VK_NULL_HANDLE);
        if (cr != VK_SUCCESS) {
            core::Log::ErrorF("Vulkan SubmitCommandList: compute acquire+list submit failed (0x%x)", cr);
            NoteVulkanDeviceLost("SubmitCommandList vkQueueSubmit (compute acquire+list)", static_cast<int>(cr));
        } else if (signalFinalTimeline)
            m_lastDeferredSubmitTimelineValue = signalValue;
        if (signalValue != 0)
            cl->ClearPendingTimelineValue();
        return;
    }

    const bool waitTimeline = needTimelineSync && (waitValue != 0);
    const bool signalTimeline = needTimelineSync && (signalValue != 0);
    if (signalTimeline && waitTimeline && signalValue <= waitValue) {
        signalValue = waitValue + 1;
        if (signalValue > m_timelineValue)
            m_timelineValue = signalValue;
        core::Log::WarnF("Vulkan SubmitCommandList: adjusted timeline signal value to preserve ordering (wait=%llu signal=%llu).",
                         static_cast<unsigned long long>(waitValue),
                         static_cast<unsigned long long>(signalValue));
    }
    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkTimelineSemaphoreSubmitInfo timelineInfo = {};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.waitSemaphoreValueCount = waitTimeline ? 1u : 0u;
    timelineInfo.pWaitSemaphoreValues = waitTimeline ? &waitValue : nullptr;
    timelineInfo.signalSemaphoreValueCount = signalTimeline ? 1u : 0u;
    timelineInfo.pSignalSemaphoreValues = &signalValue;
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = needTimelineSync ? &timelineInfo : nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buf;
    submitInfo.waitSemaphoreCount = waitTimeline ? 1u : 0u;
    submitInfo.pWaitSemaphores = waitTimeline ? &timelineSem : nullptr;
    submitInfo.pWaitDstStageMask = waitTimeline ? &waitStageMask : nullptr;
    submitInfo.signalSemaphoreCount = signalTimeline ? 1u : 0u;
    submitInfo.pSignalSemaphores = signalTimeline ? &timelineSem : nullptr;
    VkQueue submitQueue = static_cast<VkQueue>(m_queues.GetGraphicsQueue());
    const bool canUseComputeQueue = m_caps.supportsAsyncCompute && wantsComputeQueue && m_queues.GetComputeQueue() &&
                                    (!hasDedicatedComputeFamily || !requiresOwnershipTransfers);
    if (canUseComputeQueue)
        submitQueue = static_cast<VkQueue>(m_queues.GetComputeQueue());
    VkResult submitRes = vkQueueSubmit(submitQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (submitRes != VK_SUCCESS) {
        core::Log::ErrorF("Vulkan SubmitCommandList: vkQueueSubmit failed (0x%x)", submitRes);
        NoteVulkanDeviceLost("SubmitCommandList vkQueueSubmit", static_cast<int>(submitRes));
    } else if (signalTimeline)
        m_lastDeferredSubmitTimelineValue = signalValue;
    if (signalValue != 0)
        cl->ClearPendingTimelineValue();
}

void* DeviceVK::GetBarrierTracker() {
    return &m_barrierTracker;
}

void DeviceVK::TransitionTextureToState(ITexture* texture, ResourceState newState) {
    if (!texture) return;
    TextureVK* texVK = static_cast<TextureVK*>(texture);
    if (texVK->m_retired || !texVK->m_image) return;
    VkImageAspectFlags aspect = static_cast<VkImageAspectFlags>(ToVkImageAspect(texVK->GetFormat()));
    const uint32_t mipLevels = (std::max)(1u, texVK->m_mipLevels);
    uint32_t arrayLayers = 1u;
    switch (texVK->m_type) {
    case TextureType::TextureCube:
        arrayLayers = 6u;
        break;
    case TextureType::Texture2DArray:
        arrayLayers = (std::max)(1u, texVK->m_depthOrArray);
        break;
    default:
        break;
    }
    BarrierTrackerVK::TransitionImage(&m_barrierTracker, texVK->m_image, aspect, 0u, mipLevels, 0u, arrayLayers, newState, true);
}

void DeviceVK::FlushBarrierTracker(ICommandList* cmd) {
    if (!cmd) return;
    CommandListVK* cmdVK = static_cast<CommandListVK*>(cmd);
    // Dynamic rendering: vkCmdPipelineBarrier must not run inside an active rendering scope
    // when transitioning attachments. Frame graph runs transitions between passes; end rendering first.
    cmdVK->EndRenderingIfActive();
    BarrierTrackerVK::FlushBarriers(&m_barrierTracker, cmdVK);
}

IBuffer* DeviceVK::CreateBuffer(const BufferDesc& desc, const void* initialData) {
    IBuffer* buf = CreateBufferVK(this, desc, initialData);
    if (buf && desc.debugName) {
        BufferVK* b = static_cast<BufferVK*>(buf);
        SetDebugNameBuffer(this, b->m_buffer, desc.debugName);
    }
    return buf;
}

void DeviceVK::DestroyBuffer(IBuffer* buffer) {
    if (!buffer) return;
    BufferVK* b = static_cast<BufferVK*>(buffer);
    if (b->m_buffer) {
        BarrierTrackerVK::RemoveBufferState(&m_barrierTracker, b->m_buffer);
        m_deletionQueue.EnqueueBuffer(GetCurrentFrameIndex(), GetAllocator(), b->m_buffer, b->m_allocation);
        b->m_buffer = nullptr;
        b->m_allocation = nullptr;
    }
    delete b;
}

uint64_t DeviceVK::GetBufferDeviceAddress(IBuffer* buffer) {
    if (!buffer) {
        core::Log::Error("DeviceVK::GetBufferDeviceAddress: buffer is null.");
        return 0;
    }
    if (!m_device) {
        core::Log::Error("DeviceVK::GetBufferDeviceAddress: device is null.");
        return 0;
    }
    if (!m_supportsBufferDeviceAddress) {
        core::Log::Error("DeviceVK::GetBufferDeviceAddress: buffer device address is not supported/enabled on this device.");
        return 0;
    }
    auto* b = static_cast<BufferVK*>(buffer);
    VkBuffer vkBuffer = static_cast<VkBuffer>(b->m_buffer);
    if (vkBuffer == VK_NULL_HANDLE) {
        core::Log::Error("DeviceVK::GetBufferDeviceAddress: VkBuffer handle is null.");
        return 0;
    }
    if (!b->m_deviceAddressCapable) {
        core::Log::Error("DeviceVK::GetBufferDeviceAddress: buffer was created without SHADER_DEVICE_ADDRESS usage.");
        return 0;
    }
    VkBufferDeviceAddressInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = vkBuffer;
    return static_cast<uint64_t>(vkGetBufferDeviceAddress(static_cast<VkDevice>(m_device), &info));
}

ITexture* DeviceVK::CreateTexture(const TextureDesc& desc, const void* initialData) {
    ITexture* t = CreateTextureVK(this, desc, initialData);
    (void)initialData;
    return t;
}

void DeviceVK::DestroyTexture(ITexture* texture) {
    if (!texture) return;
    TextureVK* t = static_cast<TextureVK*>(texture);
    {
        std::lock_guard<std::mutex> lock(m_sharedBindlessMutex);
        m_sharedBindlessTextureHandles.erase(texture);
    }
    if (t->m_retired) return;
    const uint32_t frameIndex = GetCurrentFrameIndex();
    for (void* sliceView : t->m_slice2DViewsByLayer) {
        if (sliceView)
            m_deletionQueue.EnqueueImageView(frameIndex, m_device, sliceView);
    }
    t->m_slice2DViewsByLayer.clear();
    if (t->m_imageView)
        m_deletionQueue.EnqueueImageView(frameIndex, m_device, t->m_imageView);
    if (t->m_image) {
        BarrierTrackerVK::RemoveImageState(&m_barrierTracker, t->m_image);
        m_deletionQueue.EnqueueImage(frameIndex, GetAllocator(), t->m_image, t->m_allocation);
    }
    t->m_retired = true;
    t->m_imageView = nullptr;
    t->m_image = nullptr;
    t->m_allocation = nullptr;
    // Keep the wrapper alive for a few frames so stale CPU pointers fail safely
    // (m_image == null) instead of dereferencing freed memory.
    m_pendingTextureWrapperRetires.push_back({ t, m_frameNumber + kMaxFrames });
}

ITexture* DeviceVK::GetFallbackShaderResourceTexture2D() {
    if (m_fallbackShaderResourceTexture2D) return m_fallbackShaderResourceTexture2D;
    TextureDesc desc{};
    desc.type = TextureType::Texture2D;
    desc.width = 1;
    desc.height = 1;
    desc.format = RHITextureFormat::RGBA8;
    desc.usage = TextureUsage::ShaderResource;
    desc.mipLevels = 1;
    static const uint8_t black[4] = {0, 0, 0, 255};
    m_fallbackShaderResourceTexture2D = CreateTexture(desc, black);
    return m_fallbackShaderResourceTexture2D;
}

ITexture* DeviceVK::GetFallbackShaderResourceTextureCube() {
    if (m_fallbackShaderResourceTextureCube) return m_fallbackShaderResourceTextureCube;
    TextureDesc desc{};
    desc.type = TextureType::TextureCube;
    desc.width = 1;
    desc.height = 1;
    desc.depthOrArrayLayers = 6;
    desc.format = RHITextureFormat::RGBA8;
    desc.usage = TextureUsage::ShaderResource;
    desc.mipLevels = 1;
    static const uint8_t face[4] = {0, 0, 0, 255};
    uint8_t cubeFaces[6 * 4];
    for (uint32_t f = 0; f < 6; ++f)
        std::memcpy(cubeFaces + f * 4, face, 4);
    m_fallbackShaderResourceTextureCube = CreateTexture(desc, cubeFaces);
    return m_fallbackShaderResourceTextureCube;
}

void DeviceVK::RetirePendingTextureWrappers(bool forceAll) {
    if (m_pendingTextureWrapperRetires.empty()) return;
    const uint32_t currentFrame = m_frameNumber;
    size_t write = 0;
    for (size_t read = 0; read < m_pendingTextureWrapperRetires.size(); ++read) {
        RetiredTextureEntry& e = m_pendingTextureWrapperRetires[read];
        const bool canRetire = forceAll || (currentFrame >= e.retireFrameNumber);
        if (canRetire) {
            delete e.texture;
            e.texture = nullptr;
        } else {
            if (write != read) m_pendingTextureWrapperRetires[write] = e;
            ++write;
        }
    }
    m_pendingTextureWrapperRetires.resize(write);
}

void DeviceVK::CommitSparseTexturePages(ITexture* texture, uint32_t level, int32_t x, int32_t y, int32_t z, uint32_t width, uint32_t height, uint32_t depth, bool commit) {
    (void)texture; (void)level; (void)x; (void)y; (void)z; (void)width; (void)height; (void)depth; (void)commit;
}

void DeviceVK::CommitSparseBufferPages(IBuffer* buffer, size_t offset, size_t size, bool commit) {
    (void)buffer; (void)offset; (void)size; (void)commit;
}

ITexture* DeviceVK::CreateTextureView(ITexture* source, RHITextureFormat viewFormat, uint32_t baseMipLevel, uint32_t mipLevels, uint32_t baseLayer, uint32_t layerCount) {
    (void)source; (void)viewFormat; (void)baseMipLevel; (void)mipLevels; (void)baseLayer; (void)layerCount;
    return nullptr;
}

ITexture* DeviceVK::CreateTextureBuffer(IBuffer* buffer, RHITextureFormat format, size_t offset, size_t size) {
    (void)buffer; (void)format; (void)offset; (void)size;
    return nullptr;
}

bool DeviceVK::WaitFence(IFence* fence, uint64_t timeoutMs) {
    return WaitFenceVK(this, fence, timeoutMs);
}

void DeviceVK::DestroyFence(IFence* fence) { DestroyFenceVK(fence); }

IShader* DeviceVK::CreateShader(ShaderStage stage, const char* source, size_t sourceLen) {
    return CreateShaderVK(this, stage, source, sourceLen);
}

IShader* DeviceVK::CreateShaderFromBinary(ShaderStage stage, const void* data, size_t size) {
    return CreateShaderFromBinaryVK(this, stage, data, size);
}

bool DeviceVK::TryLoadShaderFromPaths(const char* vertexPath, const char* fragmentPath, IShader*& outVs, IShader*& outFs) {
    return TryLoadShaderPairVK(this, vertexPath, fragmentPath, outVs, outFs);
}

void DeviceVK::DestroyShader(IShader* shader) {
    DestroyShaderVK(this, shader);
}

IPipeline* DeviceVK::CreateGraphicsPipeline(const PipelineDesc& desc) {
    return CreateGraphicsPipelineVK(this, desc);
}

IPipeline* DeviceVK::CreateGraphicsPipelineFromShaderAsset(const ShaderGraphicsPipelineCreateInfo& info) {
    return CreateGraphicsPipelineFromShaderAssetVK(this, info);
}

IPipeline* DeviceVK::CreateGraphicsPipelineFromBinary(const void* data, size_t size, uint32_t binaryFormat, const VertexLayout& layout, PrimitiveTopology topology) {
    (void)data; (void)size; (void)binaryFormat; (void)layout; (void)topology;
    return nullptr;
}

bool DeviceVK::GetPipelineBinary(IPipeline* pipeline, std::vector<uint8_t>& outData, uint32_t& outFormat) {
    (void)pipeline; (void)outData; (void)outFormat;
    return false;
}

IPipeline* DeviceVK::CreateComputePipeline(void* computeShader) {
    return CreateComputePipelineVK(this, computeShader);
}

IPipeline* DeviceVK::CreateComputePipelineWithLayout(void* computeShader, PipelineLayoutKind layoutKind) {
    return CreateComputePipelineVK(this, computeShader, layoutKind);
}

void DeviceVK::DestroyPipeline(IPipeline* pipeline) {
    if (!pipeline) return;
    PipelineVK* p = static_cast<PipelineVK*>(pipeline);
    if (p->m_pipeline) {
        void* handle = p->m_pipeline;
        if (!p->m_isCompute && p->m_graphicsCacheHash != 0)
            ReleaseGraphicsPipeline(p->m_graphicsCacheHash, handle);
        else
            m_deletionQueue.EnqueuePipeline(GetCurrentFrameIndex(), m_device, handle);
        p->m_pipeline = nullptr;
    }
    p->m_graphicsCacheHash = 0;
    delete p;
}

void DeviceVK::EnsureComputePipelineStorageBlockBindings(IPipeline* pipeline) { (void)pipeline; }

bool DeviceVK::ComputePipelineHasStorageBlock(IPipeline* pipeline, const char* blockName) {
    (void)pipeline; (void)blockName;
    return true;
}

IRenderTarget* DeviceVK::CreateRenderTarget(uint32_t width, uint32_t height,
                                            const std::vector<RHITextureFormat>& colorFormats,
                                            RHITextureFormat depthFormat,
                                            uint32_t samples,
                                            bool transient) {
    return CreateRenderTargetVK(this, width, height, colorFormats, depthFormat, samples, transient);
}

IRenderTarget* DeviceVK::CreateDepthOnlyRenderTargetArray(uint32_t width, uint32_t height, uint32_t arrayLayers) {
    return CreateDepthOnlyRenderTargetArrayVK(this, width, height, arrayLayers);
}

IRenderTarget* DeviceVK::GetSharedShadowMapArrayDummyRenderTarget() {
    if (!m_sharedShadowMapArrayDummyRt) {
        m_sharedShadowMapArrayDummyRt = CreateDepthOnlyRenderTargetArray(1u, 1u, 1u);
        m_sharedShadowMapArrayDummyBarrierDone = false;
    }
    return m_sharedShadowMapArrayDummyRt;
}

void DeviceVK::EnsureSharedShadowMapArrayDummyForShaderSampling(ICommandList* cmd) {
    if (!cmd || m_sharedShadowMapArrayDummyBarrierDone)
        return;
    IRenderTarget* rt = GetSharedShadowMapArrayDummyRenderTarget();
    if (!rt)
        return;
    ITexture* depth = rt->GetDepthTexture();
    if (!depth)
        return;
    cmd->BarrierTexture(depth, ResourceState::Undefined, ResourceState::ShaderResource);
    m_sharedShadowMapArrayDummyBarrierDone = true;
}

IRenderTarget* DeviceVK::CreateRenderTargetWithSharedDepth(uint32_t width, uint32_t height,
                                                           const std::vector<RHITextureFormat>& colorFormats,
                                                           ITexture* sharedDepthTexture,
                                                           uint32_t samples) {
    return CreateRenderTargetWithSharedDepthVK(this, width, height, colorFormats, sharedDepthTexture, samples);
}

IRenderTarget* DeviceVK::CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) {
    return CreateRenderTargetFromTextureVK(this, colorTexture, face, mipLevel);
}

IRenderTarget* DeviceVK::CreateRenderTargetFromTextureWithDepth(ITexture* colorTexture, uint32_t face, uint32_t mipLevel, ITexture* depthTexture) {
    return CreateRenderTargetFromTextureWithDepthVK(this, colorTexture, face, mipLevel, depthTexture);
}

void DeviceVK::DestroyRenderTarget(IRenderTarget* rt) {
    if (rt == m_sharedShadowMapArrayDummyRt) {
        m_sharedShadowMapArrayDummyRt = nullptr;
        m_sharedShadowMapArrayDummyBarrierDone = false;
    }
    DestroyRenderTargetVK(this, rt);
}

ITexture* DeviceVK::CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) {
    (void)nativeTextureId; (void)width; (void)height; (void)format;
    return nullptr;
}

IQueryPool* DeviceVK::CreateQueryPool(QueryType type, uint32_t count, PipelineStatisticsFlags pipelineStatisticsMask) {
    if (count == 0) {
        core::Log::Error("DeviceVK::CreateQueryPool: count must be > 0.");
        return nullptr;
    }
    if (type != QueryType::Timestamp && type != QueryType::Occlusion && type != QueryType::PipelineStatistics) {
        core::Log::Error("DeviceVK::CreateQueryPool: unsupported query type.");
        return nullptr;
    }
    if (type == QueryType::Timestamp && !m_caps.supportsGpuTimestampQueries) {
        core::Log::Error("DeviceVK::CreateQueryPool: timestamp queries are unsupported on this device.");
        return nullptr;
    }
    if (type == QueryType::Occlusion && !m_caps.supportsOcclusionQueries) {
        core::Log::Error("DeviceVK::CreateQueryPool: occlusion queries are unsupported on this device.");
        return nullptr;
    }
    if (type == QueryType::PipelineStatistics) {
        if (!m_caps.supportsPipelineStatisticsQueries) {
            core::Log::Error("DeviceVK::CreateQueryPool: pipeline statistics queries are unsupported on this device.");
            return nullptr;
        }
        if (pipelineStatisticsMask != 0 && (pipelineStatisticsMask & kCorePipelineStatisticsMask) == 0) {
            core::Log::Error("DeviceVK::CreateQueryPool: pipeline statistics mask becomes empty after core-mask sanitization.");
            return nullptr;
        }
    } else if (pipelineStatisticsMask != 0) {
        core::Log::Error("DeviceVK::CreateQueryPool: pipelineStatisticsMask is only valid for QueryType::PipelineStatistics.");
        return nullptr;
    }
    return CreateQueryPoolVK(this, type, count, pipelineStatisticsMask);
}

void DeviceVK::DestroyQueryPool(IQueryPool* pool) { DestroyQueryPoolVK(pool); }

void DeviceVK::ResetQueryPoolOnHost(IQueryPool* pool, uint32_t firstQuery, uint32_t queryCount) {
    if (!m_supportsHostQueryReset) {
        core::Log::Error("DeviceVK::ResetQueryPoolOnHost: host query reset is not supported.");
        return;
    }
    if (!m_device) {
        core::Log::Error("DeviceVK::ResetQueryPoolOnHost: VkDevice is null.");
        return;
    }
    if (!pool) {
        core::Log::Error("DeviceVK::ResetQueryPoolOnHost: query pool is null.");
        return;
    }
    if (queryCount == 0) {
        core::Log::Error("DeviceVK::ResetQueryPoolOnHost: queryCount must be > 0.");
        return;
    }
    auto* qvk = static_cast<QueryPoolVK*>(pool);
    VkQueryPool qp = static_cast<VkQueryPool>(qvk->m_queryPool);
    if (qp == VK_NULL_HANDLE) return;
    const uint64_t end = static_cast<uint64_t>(firstQuery) + static_cast<uint64_t>(queryCount);
    if (end > static_cast<uint64_t>(qvk->m_count)) {
        core::Log::ErrorF("DeviceVK::ResetQueryPoolOnHost: range exceeds pool count (first=%u count=%u poolCount=%u).", firstQuery,
                          queryCount, qvk->m_count);
        return;
    }
    vkResetQueryPool(static_cast<VkDevice>(m_device), qp, firstQuery, queryCount);
}

IPipelineLayout* DeviceVK::CreatePipelineLayout(const PipelineLayoutDesc& desc) {
    return CreatePipelineLayoutVK(this, desc);
}

IPipelineLayout* DeviceVK::GetResourceSetLayout(ResourceSetType type) {
    size_t i = static_cast<size_t>(type);
    return (i < 10) ? m_resourceSetLayouts[i] : nullptr;
}

IPipelineLayout* DeviceVK::GetBindlessPipelineLayout() const {
    if (!m_bindlessSetLayout) return nullptr;
    if (!m_bindlessPipelineLayoutWrapper) {
        PipelineLayoutDesc bindlessDesc;
        bindlessDesc.bindings = {
            {0, BindingType::Texture},
            {1, BindingType::StorageBuffer},
        };
        m_bindlessPipelineLayoutWrapper = CreatePipelineLayoutWrapperVK(const_cast<DeviceVK*>(this), m_bindlessSetLayout, bindlessDesc);
    }
    return m_bindlessPipelineLayoutWrapper;
}

void DeviceVK::DestroyPipelineLayout(IPipelineLayout* layout) {
    if (!layout) return;
    PipelineLayoutVK* pl = static_cast<PipelineLayoutVK*>(layout);
    const uint32_t frameIndex = GetCurrentFrameIndex();
    if (pl->m_descriptorSetLayout && pl->m_ownsSetLayout)
        m_deletionQueue.EnqueueDescriptorSetLayout(frameIndex, m_device, pl->m_descriptorSetLayout);
    if (pl->m_pipelineLayout)
        m_deletionQueue.EnqueuePipelineLayout(frameIndex, m_device, pl->m_pipelineLayout);
    pl->m_descriptorSetLayout = nullptr;
    pl->m_pipelineLayout = nullptr;
    delete pl;
}

IDescriptorSet* DeviceVK::CreateDescriptorSet(IPipelineLayout* layout) {
    return CreateDescriptorSetVK(this, layout);
}

void DeviceVK::DestroyDescriptorSet(IDescriptorSet* set) {
    // Vulkan descriptor sets may still be referenced by submitted command buffers.
    // Route all destruction through deferred retirement to avoid freeing in-flight sets.
    EnqueueDestroyDescriptorSet(set);
}

void DeviceVK::EnqueueDestroyDescriptorSet(IDescriptorSet* set) {
    if (!set) return;
    if (std::find(m_pendingDescriptorSetsToFree.begin(), m_pendingDescriptorSetsToFree.end(), set) != m_pendingDescriptorSetsToFree.end())
        return;
    m_pendingDescriptorSetsToFree.push_back(set);
    m_ticksUntilDescriptorSetFree = 0;
}

IBindlessHeap* DeviceVK::CreateBindlessHeap() { return new BindlessHeapVK(this); }

void DeviceVK::DestroyBindlessHeap(IBindlessHeap* heap) { if (heap) delete static_cast<BindlessHeapVK*>(heap); }

IBindlessHeap* DeviceVK::GetOrCreateSharedBindlessHeap() {
    if (!m_caps.supportsBindless)
        return nullptr;
    std::lock_guard<std::mutex> lock(m_sharedBindlessMutex);
    if (!m_sharedBindlessHeap)
        m_sharedBindlessHeap = CreateBindlessHeap();
    return m_sharedBindlessHeap;
}

BindlessHandle DeviceVK::GetOrAllocateSharedBindlessTextureHandle(ITexture* texture) {
    if (!texture || !m_caps.supportsBindless)
        return kInvalidBindlessHandle;
    std::lock_guard<std::mutex> lock(m_sharedBindlessMutex);
    if (!m_sharedBindlessHeap)
        m_sharedBindlessHeap = CreateBindlessHeap();
    IBindlessHeap* heap = m_sharedBindlessHeap;
    if (!heap)
        return kInvalidBindlessHandle;
    auto it = m_sharedBindlessTextureHandles.find(texture);
    if (it != m_sharedBindlessTextureHandles.end())
        return it->second;
    const BindlessHandle handle = heap->AllocateTextureHandle(texture);
    if (handle != kInvalidBindlessHandle)
        m_sharedBindlessTextureHandles.emplace(texture, handle);
    return handle;
}

int DeviceVK::GetMaxSamples(int requestedSamples) { return std::min(requestedSamples, 4); }

} // namespace rhi
} // namespace rendering
} // namespace eyelib
