#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/render/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/render/rhi/Vulkan/pipelines/ShaderVK.h"
#include "eyelib/render/rhi/Vulkan/pipelines/PipelineVK.h"
#include "eyelib/render/rhi/Vulkan/pipelines/PipelineLayoutVK.h"
#include "eyelib/render/rhi/Vulkan/descriptors/DescriptorSetVK.h"
#include "eyelib/render/rhi/Vulkan/resources/RenderTargetVK.h"
#include "eyelib/render/rhi/Vulkan/sync/QueryPoolVK.h"
#include "eyelib/render/rhi/Vulkan/sync/FenceVK.h"
#include "eyelib/render/rhi/Vulkan/descriptors/BindlessHeapVK.h"
#include "eyelib/render/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/render/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/render/rhi/Vulkan/pipelines/VulkanPipelineHash.h"
#include "eyelib/render/rhi/Vulkan/pipelines/VulkanBindingUtils.h"
#include "eyelib/render/ShaderPathResolve.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/core/Log.h"
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <set>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace {
const VkDescriptorPoolSize kDescriptorPoolSizes[] = {
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256 },
};
const uint32_t kDescriptorPoolSizeCount = static_cast<uint32_t>(sizeof(kDescriptorPoolSizes) / sizeof(kDescriptorPoolSizes[0]));
const uint32_t kDescriptorMaxSetsPerPool = 256;

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
namespace render {
namespace rhi {

DeviceVK::DeviceVK() : m_caps{} {
    m_immediateCommandList = std::make_unique<CommandListVK>(this);
    QueryCapabilities();
}

DeviceVK::~DeviceVK() {
    if (m_fullscreenQuadVBO) { DestroyBuffer(m_fullscreenQuadVBO); m_fullscreenQuadVBO = nullptr; }
    if (m_fullscreenQuadIBO) { DestroyBuffer(m_fullscreenQuadIBO); m_fullscreenQuadIBO = nullptr; }
    m_immediateCommandList.reset();
    VkDevice device = static_cast<VkDevice>(m_device);
    if (device != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < kMaxFrames; ++i)
            FrameContextVK::Destroy(&m_frameContexts[i], device);
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
    for (auto& kv : m_graphicsPipelineCache) {
        if (kv.second != VK_NULL_HANDLE && device != VK_NULL_HANDLE)
            vkDestroyPipeline(device, static_cast<VkPipeline>(kv.second), nullptr);
    }
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
    m_legacySet0 = nullptr;  // freed with descriptor pool
    VkDescriptorSetLayout legacyLayout = static_cast<VkDescriptorSetLayout>(m_legacySet0Layout);
    if (legacyLayout != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, legacyLayout, nullptr);
        m_legacySet0Layout = nullptr;
    }
    VkDescriptorPool emptyBindlessPool = static_cast<VkDescriptorPool>(m_emptyBindlessPool);
    if (emptyBindlessPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, emptyBindlessPool, nullptr);
        m_emptyBindlessPool = nullptr;
    }
    m_emptyBindlessSet = nullptr;  // freed with pool
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
    if (device != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < kMaxFrames; ++i)
            m_deletionQueue.Tick(m_device, GetAllocator(), i);
        SwapchainVK::Destroy(&m_swapchain, device);
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

    const char* deviceExtensions[] = {
        "VK_KHR_swapchain",
    };
    uint32_t deviceExtensionCount = static_cast<uint32_t>(sizeof(deviceExtensions) / sizeof(deviceExtensions[0]));

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

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = deviceExtensionCount;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    VkDevice device = VK_NULL_HANDLE;
    VkResult res = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateDevice failed (0x%x)", res);
        InstanceVK::Destroy(&m_instanceVK);
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }
    m_device = device;

    if (!QueuesVK::Create(&m_queues, device, graphicsFamily, computeFamily, transferFamily)) {
        core::Log::Error("DeviceVK::Initialize: QueuesVK::Create failed");
        vkDestroyDevice(device, nullptr);
        InstanceVK::Destroy(&m_instanceVK);
        m_device = nullptr;
        PhysicalDeviceVK::Destroy(&m_physicalDeviceVK);
        return false;
    }

    if (!AllocatorVK::Create(&m_allocatorVK, physicalDevice, device, instance, VK_API_VERSION_1_2)) {
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

    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256 },
    };
    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCreateInfo.maxSets = 256;
    poolCreateInfo.poolSizeCount = static_cast<uint32_t>(sizeof(poolSizes) / sizeof(poolSizes[0]));
    poolCreateInfo.pPoolSizes = poolSizes;
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

    VkDescriptorSetLayout legacySet0Layout = VK_NULL_HANDLE;
    VkDescriptorSet legacySet0 = VK_NULL_HANDLE;
    if (!vkhelpers::CreateLegacySet0(device, descriptorPool, &legacySet0Layout, &legacySet0)) {
        core::Log::Error("DeviceVK::Initialize: failed to create legacy set0 resources");
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
        vkDestroySampler(device, defaultSampler, nullptr);
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
    m_legacySet0Layout = legacySet0Layout;
    m_legacySet0 = legacySet0;

    VkDescriptorSetLayout computeSet0Layout = VK_NULL_HANDLE;
    VkDescriptorSet computeSet0 = VK_NULL_HANDLE;
    if (!vkhelpers::CreateComputeSet0(device, descriptorPool, &computeSet0Layout, &computeSet0)) {
        core::Log::Error("DeviceVK::Initialize: failed to create compute set0 resources");
        vkDestroyDescriptorSetLayout(device, legacySet0Layout, nullptr);
        m_legacySet0Layout = nullptr;
        m_legacySet0 = nullptr;
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
    bindlessLayoutInfo.bindingCount = 2;
    bindlessLayoutInfo.pBindings = bindlessBindings;
    VkDescriptorSetLayout bindlessSetLayout = VK_NULL_HANDLE;
    res = vkCreateDescriptorSetLayout(device, &bindlessLayoutInfo, nullptr, &bindlessSetLayout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateDescriptorSetLayout (bindless) failed (0x%x)", res);
        vkDestroyDescriptorSetLayout(device, legacySet0Layout, nullptr);
        m_legacySet0Layout = nullptr;
        m_legacySet0 = nullptr;
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

    VkPipelineLayout defaultPipelineLayout = VK_NULL_HANDLE;
    if (!vkhelpers::CreateDefaultPipelineLayout(device, legacySet0Layout, bindlessSetLayout, &defaultPipelineLayout)) {
        core::Log::Error("DeviceVK::Initialize: failed to create default pipeline layout");
        vkDestroyDescriptorSetLayout(device, legacySet0Layout, nullptr);
        m_legacySet0Layout = nullptr;
        m_legacySet0 = nullptr;
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
    if (!vkhelpers::CreateComputePipelineLayout(device, computeSet0Layout, bindlessSetLayout, &computePipelineLayout)) {
        core::Log::Error("DeviceVK::Initialize: failed to create compute pipeline layout");
        vkDestroyPipelineLayout(device, defaultPipelineLayout, nullptr);
        m_defaultPipelineLayout = nullptr;
        vkDestroyDescriptorSetLayout(device, bindlessSetLayout, nullptr);
        m_bindlessSetLayout = nullptr;
        vkDestroyDescriptorSetLayout(device, legacySet0Layout, nullptr);
        m_legacySet0Layout = nullptr;
        m_legacySet0 = nullptr;
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
        if (!SwapchainVK::Create(&m_swapchain, device, m_physicalDeviceVK.GetHandle(), m_surface, width, height)) {
            core::Log::ErrorF("DeviceVK::Initialize: SwapchainVK::Create failed");
        }
    }

    core::Log::Info("Vulkan device initialized");
    return true;
}

void DeviceVK::QueryCapabilities() {
    m_caps = RHICapabilities{};
    m_caps.supportsCompute = true;
    m_caps.supportsMultiDrawIndirect = true;
    m_caps.supportsMultiDrawIndirectCount = true;
    m_caps.supportsAsyncCompute = true;
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

void DeviceVK::UpdateLegacySet0(CommandListVK* cmd) {
    vkhelpers::UpdateLegacySet0(this, cmd);
}

void DeviceVK::UpdateComputeSet0(CommandListVK* cmd) {
    vkhelpers::UpdateComputeSet0(this, cmd);
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
    return (it != m_graphicsPipelineCache.end()) ? it->second : nullptr;
}

void DeviceVK::RegisterGraphicsPipeline(uint64_t hash, void* pipeline) {
    if (pipeline)
        m_graphicsPipelineCache[hash] = pipeline;
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

void DeviceVK::BeginFrame() {
    if (!m_immediateCommandList || !m_device || !m_queues.GetGraphicsQueue()) return;
    m_currentFrameIndex = (m_currentFrameIndex + 1) % kMaxFrames;
    ++m_frameNumber;
    FrameContextVK& frame = m_frameContexts[m_currentFrameIndex];
    VkDevice device = static_cast<VkDevice>(m_device);
    VkFence fence = static_cast<VkFence>(frame.fence);
    VkCommandPool pool = static_cast<VkCommandPool>(frame.commandPool);
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(frame.commandBuffer.GetHandle());
    if (fence == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || buf == VK_NULL_HANDLE) return;

    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    m_deletionQueue.Tick(m_device, GetAllocator(), m_currentFrameIndex);
    UploadHeapVK::BeginFrame(&m_uploadHeap, m_device, frame.fence, m_currentFrameIndex);
    DescriptorAllocatorVK::Reset(&m_descriptorAllocator, m_device, m_currentFrameIndex);
    DescriptorCacheVK::Reset(&m_descriptorCache, m_device, m_currentFrameIndex);
    BarrierTrackerVK::Reset(&m_barrierTracker);
    vkResetFences(device, 1, &fence);
    vkResetCommandPool(device, pool, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) return;
    m_immediateCommandList->SetCommandBuffer(buf, true);
    m_immediateCommandList->SetRecordingState(true);
    m_frameOpen = true;
}

// Submission is device-owned: only EndFrame() / SubmitCommandList() submit to the queue. Renderer only records.
void DeviceVK::EndFrame() {
    m_frameOpen = false;
    if (!m_immediateCommandList || !m_queues.GetGraphicsQueue()) return;
    CommandListVK* imm = m_immediateCommandList.get();
    imm->End();
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(imm->GetCommandBuffer());
    VkQueue queue = static_cast<VkQueue>(m_queues.GetGraphicsQueue());
    FrameContextVK& frame = m_frameContexts[m_currentFrameIndex];
    VkFence fence = static_cast<VkFence>(frame.fence);
    if (buf != VK_NULL_HANDLE && fence != VK_NULL_HANDLE) {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buf;
        VkSemaphore waitSem = static_cast<VkSemaphore>(frame.imageAvailable);
        VkSemaphore signalSem = static_cast<VkSemaphore>(frame.renderFinished);
        if (m_pendingPresent && waitSem != VK_NULL_HANDLE && signalSem != VK_NULL_HANDLE) {
            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &waitSem;
            submitInfo.pWaitDstStageMask = &waitStage;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &signalSem;
        }
        vkQueueSubmit(queue, 1, &submitInfo, fence);
    }
}

void DeviceVK::SubmitCommandList(ICommandList* list) {
    if (!list || !m_queues.GetGraphicsQueue()) return;
    CommandListVK* cl = static_cast<CommandListVK*>(list);
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(cl->GetCommandBuffer());
    if (buf == VK_NULL_HANDLE) return;
    VkSemaphore timelineSem = static_cast<VkSemaphore>(m_timelineSemaphore);
    uint64_t signalValue = cl->GetPendingTimelineValue();
    VkTimelineSemaphoreSubmitInfo timelineInfo = {};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.signalSemaphoreValueCount = (signalValue != 0) ? 1u : 0u;
    timelineInfo.pSignalSemaphoreValues = &signalValue;
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = (signalValue != 0) ? &timelineInfo : nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buf;
    submitInfo.signalSemaphoreCount = (signalValue != 0) ? 1u : 0u;
    submitInfo.pSignalSemaphores = (signalValue != 0) ? &timelineSem : nullptr;
    vkQueueSubmit(static_cast<VkQueue>(m_queues.GetGraphicsQueue()), 1, &submitInfo, VK_NULL_HANDLE);
    if (signalValue != 0)
        cl->ClearPendingTimelineValue();
}

void* DeviceVK::GetBarrierTracker() {
    return &m_barrierTracker;
}

void DeviceVK::TransitionTextureToState(ITexture* texture, ResourceState newState) {
    if (!texture) return;
    TextureVK* texVK = static_cast<TextureVK*>(texture);
    if (!texVK->m_image) return;
    VkImageAspectFlags aspect = static_cast<VkImageAspectFlags>(ToVkImageAspect(texVK->GetFormat()));
    BarrierTrackerVK::TransitionImage(&m_barrierTracker, texVK->m_image, aspect, newState);
}

void DeviceVK::FlushBarrierTracker(ICommandList* cmd) {
    if (!cmd) return;
    CommandListVK* cmdVK = static_cast<CommandListVK*>(cmd);
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
        m_deletionQueue.EnqueueBuffer(GetCurrentFrameIndex(), GetAllocator(), b->m_buffer, b->m_allocation);
        b->m_buffer = nullptr;
        b->m_allocation = nullptr;
    }
    delete b;
}

ITexture* DeviceVK::CreateTexture(const TextureDesc& desc, const void* initialData) {
    return CreateTextureVK(this, desc, initialData);
}

void DeviceVK::DestroyTexture(ITexture* texture) {
    if (!texture) return;
    TextureVK* t = static_cast<TextureVK*>(texture);
    const uint32_t frameIndex = GetCurrentFrameIndex();
    if (t->m_imageView)
        m_deletionQueue.EnqueueImageView(frameIndex, m_device, t->m_imageView);
    if (t->m_image)
        m_deletionQueue.EnqueueImage(frameIndex, GetAllocator(), t->m_image, t->m_allocation);
    t->m_imageView = nullptr;
    t->m_image = nullptr;
    t->m_allocation = nullptr;
    delete t;
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

IPipeline* DeviceVK::CreateGraphicsPipelineFromName(const char* shaderName, const VertexLayout& layout, PrimitiveTopology topology, const DepthStencilState* depthStencilOverride) {
    return CreateGraphicsPipelineFromNameVK(this, shaderName, layout, topology, depthStencilOverride);
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

void DeviceVK::DestroyPipeline(IPipeline* pipeline) {
    if (!pipeline) return;
    PipelineVK* p = static_cast<PipelineVK*>(pipeline);
    if (p->m_pipeline) {
        m_deletionQueue.EnqueuePipeline(GetCurrentFrameIndex(), m_device, p->m_pipeline);
        p->m_pipeline = nullptr;
    }
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
    DestroyRenderTargetVK(this, rt);
}

ITexture* DeviceVK::CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) {
    (void)nativeTextureId; (void)width; (void)height; (void)format;
    return nullptr;
}

IQueryPool* DeviceVK::CreateQueryPool(QueryType type, uint32_t count) {
    return CreateQueryPoolVK(this, type, count);
}

void DeviceVK::DestroyQueryPool(IQueryPool* pool) { DestroyQueryPoolVK(pool); }

IPipelineLayout* DeviceVK::CreatePipelineLayout(const PipelineLayoutDesc& desc) {
    return CreatePipelineLayoutVK(this, desc);
}

void DeviceVK::DestroyPipelineLayout(IPipelineLayout* layout) {
    if (!layout) return;
    PipelineLayoutVK* pl = static_cast<PipelineLayoutVK*>(layout);
    const uint32_t frameIndex = GetCurrentFrameIndex();
    if (pl->m_descriptorSetLayout)
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
    DestroyDescriptorSetVK(this, set);
}

IBindlessHeap* DeviceVK::CreateBindlessHeap() { return new BindlessHeapVK(this); }

void DeviceVK::DestroyBindlessHeap(IBindlessHeap* heap) { if (heap) delete static_cast<BindlessHeapVK*>(heap); }

int DeviceVK::GetMaxSamples(int requestedSamples) { return std::min(requestedSamples, 4); }

} // namespace rhi
} // namespace render
} // namespace eyelib
