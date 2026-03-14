#include "eyelib/render/rhi/Vulkan/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/BufferVK.h"
#include "eyelib/render/rhi/Vulkan/TextureVK.h"
#include "eyelib/render/rhi/Vulkan/ShaderVK.h"
#include "eyelib/render/rhi/Vulkan/PipelineVK.h"
#include "eyelib/render/rhi/Vulkan/PipelineLayoutVK.h"
#include "eyelib/render/rhi/Vulkan/DescriptorSetVK.h"
#include "eyelib/render/rhi/Vulkan/RenderTargetVK.h"
#include "eyelib/render/rhi/Vulkan/QueryPoolVK.h"
#include "eyelib/render/rhi/Vulkan/FenceVK.h"
#include "eyelib/render/rhi/Vulkan/BindlessHeapVK.h"
#include "eyelib/render/rhi/Vulkan/VulkanEnums.h"
#include "eyelib/render/rhi/Vulkan/VulkanPipelineHash.h"
#include "eyelib/render/ShaderPathResolve.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/core/Log.h"
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#define VMA_IMPLEMENTATION
#include "eyelib/render/rhi/Vulkan/vk_mem_alloc.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>
#include <set>
#if defined(_WIN32)
#include <Windows.h>
#undef min
#undef max
// Local declarations so we don't rely on vulkan.h include order for VK_USE_PLATFORM_WIN32_KHR
typedef struct VkWin32SurfaceCreateInfoKHR_ {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    void* hinstance;
    void* hwnd;
} VkWin32SurfaceCreateInfoKHR_local;
typedef VkResult (VKAPI_PTR *PFN_vkCreateWin32SurfaceKHR_local)(VkInstance, const VkWin32SurfaceCreateInfoKHR_local*, const VkAllocationCallbacks*, VkSurfaceKHR*);
#endif

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
    VkCommandPool pool = static_cast<VkCommandPool>(m_commandPool);
    VkDevice device = static_cast<VkDevice>(m_device);
    if (pool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, pool, nullptr);
        m_commandPool = nullptr;
    }
    VkSampler defaultSampler = static_cast<VkSampler>(m_defaultSampler);
    if (defaultSampler != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroySampler(device, defaultSampler, nullptr);
        m_defaultSampler = nullptr;
    }
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
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
    }
    VkPipelineLayout defaultPl = static_cast<VkPipelineLayout>(m_defaultPipelineLayout);
    if (defaultPl != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, defaultPl, nullptr);
        m_defaultPipelineLayout = nullptr;
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
        for (void* p : m_swapchainImageViews) {
            if (p) vkDestroyImageView(device, static_cast<VkImageView>(p), nullptr);
        }
        m_swapchainImageViews.clear();
    }
    VkSwapchainKHR swapchain = static_cast<VkSwapchainKHR>(m_swapchain);
    if (swapchain != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        m_swapchain = nullptr;
    }
    VkInstance instanceForSurface = static_cast<VkInstance>(m_instance);
    VkSurfaceKHR surface = static_cast<VkSurfaceKHR>(m_surface);
    if (surface != VK_NULL_HANDLE && instanceForSurface != VK_NULL_HANDLE) {
        PFN_vkDestroySurfaceKHR pfnDestroySurface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
            vkGetInstanceProcAddr(instanceForSurface, "vkDestroySurfaceKHR"));
        if (pfnDestroySurface) pfnDestroySurface(instanceForSurface, surface, nullptr);
        m_surface = nullptr;
    }
    VmaAllocator allocator = static_cast<VmaAllocator>(m_allocator);
    if (allocator) {
        vmaDestroyAllocator(allocator);
        m_allocator = nullptr;
    }
    VkInstance instance = static_cast<VkInstance>(m_instance);
    if (device != VK_NULL_HANDLE)
        vkDestroyDevice(device, nullptr);
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance(instance, nullptr);
    m_device = nullptr;
    m_instance = nullptr;
    m_physicalDevice = nullptr;
    m_graphicsQueue = nullptr;
    m_computeQueue = nullptr;
    m_transferQueue = nullptr;
}

bool DeviceVK::Initialize(void* windowHandle, uint32_t width, uint32_t height) {
    m_swapchainWidth = width;
    m_swapchainHeight = height;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "OpenEye";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "OpenEye";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    std::vector<const char*> instanceExtensions = { "VK_KHR_surface" };
#if defined(_WIN32)
    instanceExtensions.push_back("VK_KHR_win32_surface");
#endif

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateInstance failed (0x%x)", res);
        return false;
    }
    m_instance = instance;

#if defined(_WIN32)
    if (windowHandle && width > 0 && height > 0) {
        auto pfnCreateWin32Surface = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR_local>(
            vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR"));
        if (pfnCreateWin32Surface) {
            VkWin32SurfaceCreateInfoKHR_local surfaceInfo = {};
            surfaceInfo.sType = static_cast<VkStructureType>(1000069000); // VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR
            surfaceInfo.hinstance = GetModuleHandleW(nullptr);
            surfaceInfo.hwnd = windowHandle;
            VkSurfaceKHR surface = VK_NULL_HANDLE;
            VkResult surfRes = pfnCreateWin32Surface(instance, &surfaceInfo, nullptr, &surface);
            if (surfRes == VK_SUCCESS)
                m_surface = surface;
            else
                core::Log::ErrorF("DeviceVK::Initialize: vkCreateWin32SurfaceKHR failed (0x%x)", surfRes);
        }
    }
#endif

    uint32_t physicalDeviceCount = 0;
    res = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    if (res != VK_SUCCESS || physicalDeviceCount == 0) {
        core::Log::ErrorF("DeviceVK::Initialize: no physical devices (0x%x)", res);
        return false;
    }
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    res = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkEnumeratePhysicalDevices failed (0x%x)", res);
        return false;
    }

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    for (VkPhysicalDevice pd : physicalDevices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(pd, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
            props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            physicalDevice = pd;
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE)
        physicalDevice = physicalDevices[0];
    m_physicalDevice = physicalDevice;

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

    m_graphicsQueueFamily = graphicsFamily;
    m_computeQueueFamily = computeFamily;
    m_transferQueueFamily = transferFamily;

    const char* deviceExtensions[] = {
        "VK_KHR_swapchain",
        "VK_KHR_dynamic_rendering",
        "VK_EXT_descriptor_indexing",
    };
    uint32_t deviceExtensionCount = static_cast<uint32_t>(sizeof(deviceExtensions) / sizeof(deviceExtensions[0]));

    VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures = {};
    timelineSemaphoreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timelineSemaphoreFeatures.timelineSemaphore = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures = {};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    indexingFeatures.pNext = &timelineSemaphoreFeatures;
    indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    indexingFeatures.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    indexingFeatures.runtimeDescriptorArray = VK_TRUE;

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
    deviceCreateInfo.pNext = &indexingFeatures;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = deviceExtensionCount;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    VkDevice device = VK_NULL_HANDLE;
    res = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateDevice failed (0x%x)", res);
        vkDestroyInstance(instance, nullptr);
        m_instance = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_device = device;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);
    vkGetDeviceQueue(device, transferFamily, 0, &transferQueue);
    m_graphicsQueue = graphicsQueue;
    m_computeQueue = computeQueue;
    m_transferQueue = transferQueue;

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    VmaAllocator allocator = VK_NULL_HANDLE;
    res = vmaCreateAllocator(&allocatorInfo, &allocator);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vmaCreateAllocator failed (0x%x)", res);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_device = nullptr;
        m_instance = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_allocator = allocator;

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    res = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateCommandPool failed (0x%x)", res);
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_allocator = nullptr;
        m_device = nullptr;
        m_instance = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_commandPool = commandPool;

    VkCommandBuffer immBuf = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    res = vkAllocateCommandBuffers(device, &allocInfo, &immBuf);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkAllocateCommandBuffers (immediate) failed (0x%x)", res);
        vkDestroyCommandPool(device, commandPool, nullptr);
        m_commandPool = nullptr;
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_allocator = nullptr;
        m_device = nullptr;
        m_instance = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_immediateCommandList->SetCommandBuffer(immBuf, true);

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
        vkFreeCommandBuffers(device, commandPool, 1, &immBuf);
        vkDestroyCommandPool(device, commandPool, nullptr);
        m_commandPool = nullptr;
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_allocator = nullptr;
        m_device = nullptr;
        m_instance = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_descriptorPool = descriptorPool;

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
        vkFreeCommandBuffers(device, commandPool, 1, &immBuf);
        vkDestroyCommandPool(device, commandPool, nullptr);
        m_commandPool = nullptr;
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_allocator = nullptr;
        m_device = nullptr;
        m_instance = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_defaultSampler = defaultSampler;

    VkPipelineCacheCreateInfo pipelineCacheInfo = {};
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    res = vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr, &pipelineCache);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreatePipelineCache failed (0x%x)", res);
        vkDestroySampler(device, defaultSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_defaultSampler = nullptr;
        m_descriptorPool = nullptr;
        vkFreeCommandBuffers(device, commandPool, 1, &immBuf);
        vkDestroyCommandPool(device, commandPool, nullptr);
        m_commandPool = nullptr;
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_allocator = nullptr;
        m_device = nullptr;
        m_instance = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_pipelineCache = pipelineCache;

    const uint32_t kLegacySet0ImageBindings[] = { 0, 1, 2, 3, 6, 11, 12, 13 };
    const uint32_t kLegacySet0StorageBindings[] = { 5, 7 };
    VkDescriptorSetLayoutBinding legacyBindings[10] = {};
    for (size_t i = 0; i < 8; i++) {
        legacyBindings[i].binding = kLegacySet0ImageBindings[i];
        legacyBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        legacyBindings[i].descriptorCount = 1;
        legacyBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    for (size_t i = 0; i < 2; i++) {
        legacyBindings[8 + i].binding = kLegacySet0StorageBindings[i];
        legacyBindings[8 + i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        legacyBindings[8 + i].descriptorCount = 1;
        legacyBindings[8 + i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo legacyLayoutInfo = {};
    legacyLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    legacyLayoutInfo.bindingCount = 10;
    legacyLayoutInfo.pBindings = legacyBindings;
    VkDescriptorSetLayout legacySet0Layout = VK_NULL_HANDLE;
    res = vkCreateDescriptorSetLayout(device, &legacyLayoutInfo, nullptr, &legacySet0Layout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreateDescriptorSetLayout (legacy set 0) failed (0x%x)", res);
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
        vkDestroySampler(device, defaultSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_descriptorPool = nullptr;
        vkFreeCommandBuffers(device, commandPool, 1, &immBuf);
        vkDestroyCommandPool(device, commandPool, nullptr);
        m_allocator = nullptr;
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_instance = nullptr;
        m_device = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_legacySet0Layout = legacySet0Layout;

    VkDescriptorSetAllocateInfo legacyAlloc = {};
    legacyAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    legacyAlloc.descriptorPool = descriptorPool;
    legacyAlloc.descriptorSetCount = 1;
    legacyAlloc.pSetLayouts = &legacySet0Layout;
    VkDescriptorSet legacySet0 = VK_NULL_HANDLE;
    res = vkAllocateDescriptorSets(device, &legacyAlloc, &legacySet0);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkAllocateDescriptorSets (legacy set 0) failed (0x%x)", res);
        vkDestroyDescriptorSetLayout(device, legacySet0Layout, nullptr);
        m_legacySet0Layout = nullptr;
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        m_pipelineCache = nullptr;
        vkDestroySampler(device, defaultSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        m_descriptorPool = nullptr;
        vkFreeCommandBuffers(device, commandPool, 1, &immBuf);
        vkDestroyCommandPool(device, commandPool, nullptr);
        m_allocator = nullptr;
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_instance = nullptr;
        m_device = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_legacySet0 = legacySet0;

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
        vkFreeCommandBuffers(device, commandPool, 1, &immBuf);
        vkDestroyCommandPool(device, commandPool, nullptr);
        m_commandPool = nullptr;
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_allocator = nullptr;
        m_device = nullptr;
        m_instance = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_bindlessSetLayout = bindlessSetLayout;

    VkDescriptorSetLayout setLayouts[2] = { legacySet0Layout, bindlessSetLayout };
    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = 128;
    VkPipelineLayoutCreateInfo defaultPlInfo = {};
    defaultPlInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    defaultPlInfo.setLayoutCount = 2;
    defaultPlInfo.pSetLayouts = setLayouts;
    defaultPlInfo.pushConstantRangeCount = 1;
    defaultPlInfo.pPushConstantRanges = &pushRange;
    VkPipelineLayout defaultPipelineLayout = VK_NULL_HANDLE;
    res = vkCreatePipelineLayout(device, &defaultPlInfo, nullptr, &defaultPipelineLayout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::Initialize: vkCreatePipelineLayout (default) failed (0x%x)", res);
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
        vkFreeCommandBuffers(device, commandPool, 1, &immBuf);
        vkDestroyCommandPool(device, commandPool, nullptr);
        m_commandPool = nullptr;
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_allocator = nullptr;
        m_device = nullptr;
        m_instance = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_defaultPipelineLayout = defaultPipelineLayout;

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
        vkFreeCommandBuffers(device, commandPool, 1, &immBuf);
        vkDestroyCommandPool(device, commandPool, nullptr);
        m_commandPool = nullptr;
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_allocator = nullptr;
        m_device = nullptr;
        m_instance = nullptr;
        m_physicalDevice = nullptr;
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
        vkFreeCommandBuffers(device, commandPool, 1, &immBuf);
        vkDestroyCommandPool(device, commandPool, nullptr);
        m_commandPool = nullptr;
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_allocator = nullptr;
        m_device = nullptr;
        m_instance = nullptr;
        m_physicalDevice = nullptr;
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
        vkFreeCommandBuffers(device, commandPool, 1, &immBuf);
        vkDestroyCommandPool(device, commandPool, nullptr);
        m_commandPool = nullptr;
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        m_allocator = nullptr;
        m_device = nullptr;
        m_instance = nullptr;
        m_physicalDevice = nullptr;
        return false;
    }
    m_timelineSemaphore = timelineSemaphore;

    if (m_surface != nullptr) {
        VkSurfaceKHR surface = static_cast<VkSurfaceKHR>(m_surface);
        VkPhysicalDevice phys = static_cast<VkPhysicalDevice>(m_physicalDevice);
        VkSurfaceCapabilitiesKHR caps = {};
        res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);
        if (res != VK_SUCCESS) {
            core::Log::ErrorF("DeviceVK::Initialize: vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed (0x%x)", res);
        } else {
            uint32_t formatCount = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr);
            std::vector<VkSurfaceFormatKHR> formats(formatCount);
            if (formatCount) vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data());
            VkSurfaceFormatKHR chosenFormat = formats.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : formats[0];
            for (const auto& f : formats) {
                if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    chosenFormat = f;
                    break;
                }
            }
            uint32_t presentModeCount = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &presentModeCount, nullptr);
            std::vector<VkPresentModeKHR> presentModes(presentModeCount);
            if (presentModeCount) vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &presentModeCount, presentModes.data());
            VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
            for (VkPresentModeKHR pm : presentModes) {
                if (pm == VK_PRESENT_MODE_FIFO_KHR) { presentMode = pm; break; }
            }
            VkExtent2D extent = {};
            extent.width = std::clamp(m_swapchainWidth, caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height = std::clamp(m_swapchainHeight, caps.minImageExtent.height, caps.maxImageExtent.height);
            uint32_t imageCount = caps.minImageCount + 1;
            if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;
            VkSwapchainCreateInfoKHR swapInfo = {};
            swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            swapInfo.surface = surface;
            swapInfo.minImageCount = imageCount;
            swapInfo.imageFormat = chosenFormat.format;
            swapInfo.imageColorSpace = chosenFormat.colorSpace;
            swapInfo.imageExtent = extent;
            swapInfo.imageArrayLayers = 1;
            swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapInfo.preTransform = caps.currentTransform;
            swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            swapInfo.presentMode = presentMode;
            swapInfo.clipped = VK_TRUE;
            VkSwapchainKHR swapchain = VK_NULL_HANDLE;
            res = vkCreateSwapchainKHR(device, &swapInfo, nullptr, &swapchain);
            if (res != VK_SUCCESS) {
                core::Log::ErrorF("DeviceVK::Initialize: vkCreateSwapchainKHR failed (0x%x)", res);
            } else {
                m_swapchain = swapchain;
                m_swapchainFormat = static_cast<uint32_t>(chosenFormat.format);
                uint32_t swapchainImageCount = 0;
                vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
                std::vector<VkImage> swapchainImages(swapchainImageCount);
                vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());
                m_swapchainImages.reserve(swapchainImageCount);
                for (VkImage img : swapchainImages) m_swapchainImages.push_back(img);
                m_swapchainImageViews.resize(swapchainImageCount);
                for (uint32_t i = 0; i < swapchainImageCount; ++i) {
                    VkImageViewCreateInfo viewInfo = {};
                    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                    viewInfo.image = swapchainImages[i];
                    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                    viewInfo.format = chosenFormat.format;
                    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    viewInfo.subresourceRange.baseMipLevel = 0;
                    viewInfo.subresourceRange.levelCount = 1;
                    viewInfo.subresourceRange.baseArrayLayer = 0;
                    viewInfo.subresourceRange.layerCount = 1;
                    VkImageView imageView = VK_NULL_HANDLE;
                    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) == VK_SUCCESS)
                        m_swapchainImageViews[i] = imageView;
                }
            }
        }
    }

    core::Log::Info("DeviceVK::Initialize: Vulkan instance, device, VMA allocator, command system, descriptor pool, pipeline cache, bindless layout, default layout, timeline semaphore created");
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
    if (!cmd || !m_legacySet0 || !m_device || !m_defaultSampler) return;
    VkDevice device = static_cast<VkDevice>(m_device);
    VkSampler sampler = static_cast<VkSampler>(m_defaultSampler);
    VkDescriptorSet set = static_cast<VkDescriptorSet>(m_legacySet0);
    const uint32_t kImageBindings[] = { 0, 1, 2, 3, 6, 11, 12, 13 };
    const uint32_t kStorageBindings[] = { 5, 7 };
    VkDescriptorImageInfo imageInfos[8] = {};
    VkDescriptorBufferInfo bufferInfos[2] = {};
    VkWriteDescriptorSet writes[10] = {};
    uint32_t numWrites = 0;
    for (size_t i = 0; i < 8; i++) {
        uint32_t b = kImageBindings[i];
        ITexture* tex = cmd->GetBoundTexture(b);
        if (!tex) continue;
        TextureVK* tVK = static_cast<TextureVK*>(tex);
        VkImageView view = static_cast<VkImageView>(tVK->m_imageView);
        if (view == VK_NULL_HANDLE) continue;
        imageInfos[i].sampler = sampler;
        imageInfos[i].imageView = view;
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        writes[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[numWrites].dstSet = set;
        writes[numWrites].dstBinding = b;
        writes[numWrites].dstArrayElement = 0;
        writes[numWrites].descriptorCount = 1;
        writes[numWrites].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[numWrites].pImageInfo = &imageInfos[i];
        numWrites++;
    }
    for (size_t i = 0; i < 2; i++) {
        uint32_t b = kStorageBindings[i];
        IBuffer* buf = nullptr;
        uint32_t offset = 0, size = 0;
        cmd->GetBoundStorageBuffer(b, &buf, &offset, &size);
        if (!buf) continue;
        BufferVK* bVK = static_cast<BufferVK*>(buf);
        VkBuffer vkBuf = static_cast<VkBuffer>(bVK->m_buffer);
        if (vkBuf == VK_NULL_HANDLE) continue;
        bufferInfos[i].buffer = vkBuf;
        bufferInfos[i].offset = offset;
        bufferInfos[i].range = size > 0 ? size : VK_WHOLE_SIZE;
        writes[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[numWrites].dstSet = set;
        writes[numWrites].dstBinding = b;
        writes[numWrites].dstArrayElement = 0;
        writes[numWrites].descriptorCount = 1;
        writes[numWrites].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[numWrites].pBufferInfo = &bufferInfos[i];
        numWrites++;
    }
    if (numWrites > 0)
        vkUpdateDescriptorSets(device, numWrites, writes, 0, nullptr);
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

void DeviceVK::FreeCommandBuffer(void* cmdBuf) {
    if (!cmdBuf || !m_commandPool || !m_device) return;
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(cmdBuf);
    vkFreeCommandBuffers(static_cast<VkDevice>(m_device), static_cast<VkCommandPool>(m_commandPool), 1, &buf);
}

ICommandList* DeviceVK::CreateCommandList() {
    if (!m_commandPool || !m_device) return nullptr;
    VkCommandBuffer buf = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = static_cast<VkCommandPool>(m_commandPool);
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkResult res = vkAllocateCommandBuffers(static_cast<VkDevice>(m_device), &allocInfo, &buf);
    if (res != VK_SUCCESS) return nullptr;
    return new CommandListVK(this, buf);
}

void DeviceVK::DestroyCommandList(ICommandList* list) {
    if (!list) return;
    CommandListVK* cl = static_cast<CommandListVK*>(list);
    if (!cl->IsImmediate())
        FreeCommandBuffer(cl->GetCommandBuffer());
    delete cl;
}

ICommandList* DeviceVK::GetImmediateCommandList() { return m_immediateCommandList.get(); }

void DeviceVK::SubmitCommandList(ICommandList* list) {
    if (!list || !m_graphicsQueue) return;
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
    vkQueueSubmit(static_cast<VkQueue>(m_graphicsQueue), 1, &submitInfo, VK_NULL_HANDLE);
    if (signalValue != 0)
        cl->ClearPendingTimelineValue();
}

IBuffer* DeviceVK::CreateBuffer(const BufferDesc& desc, const void* initialData) {
    VmaAllocator allocator = static_cast<VmaAllocator>(m_allocator);
    if (!allocator || desc.sizeInBytes == 0) return nullptr;

    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = desc.sizeInBytes;
    bufInfo.usage = ToVkBufferUsage(desc.usage);

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = (desc.memoryHint == BufferMemoryHint::HostVisible || desc.memoryHint == BufferMemoryHint::HostVisiblePersistent)
        ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY;
    if (desc.memoryHint == BufferMemoryHint::HostVisiblePersistent)
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer vkBuffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkResult res = vmaCreateBuffer(allocator, &bufInfo, &allocCreateInfo, &vkBuffer, &allocation, nullptr);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::CreateBuffer: vmaCreateBuffer failed (0x%x)", res);
        return nullptr;
    }

    auto* b = new BufferVK(this);
    b->m_size = desc.sizeInBytes;
    b->m_hint = desc.memoryHint;
    b->m_buffer = vkBuffer;
    b->m_allocation = allocation;

    if (initialData && desc.sizeInBytes > 0 &&
        (desc.memoryHint == BufferMemoryHint::HostVisible || desc.memoryHint == BufferMemoryHint::HostVisiblePersistent)) {
        void* mapped = nullptr;
        if (vmaMapMemory(allocator, allocation, &mapped) == VK_SUCCESS) {
            memcpy(mapped, initialData, desc.sizeInBytes);
            vmaUnmapMemory(allocator, allocation);
        }
    }
    return b;
}

void DeviceVK::DestroyBuffer(IBuffer* buffer) {
    if (!buffer) return;
    BufferVK* b = static_cast<BufferVK*>(buffer);
    VmaAllocator allocator = static_cast<VmaAllocator>(m_allocator);
    if (allocator && b->m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, static_cast<VkBuffer>(b->m_buffer), static_cast<VmaAllocation>(b->m_allocation));
        b->m_buffer = nullptr;
        b->m_allocation = nullptr;
    }
    delete b;
}

ITexture* DeviceVK::CreateTexture(const TextureDesc& desc, const void* initialData) {
    (void)initialData;
    VmaAllocator allocator = static_cast<VmaAllocator>(m_allocator);
    VkDevice device = static_cast<VkDevice>(m_device);
    if (!allocator || !device || desc.width == 0 || desc.height == 0) return nullptr;

    VkImageType imageType = VK_IMAGE_TYPE_2D;
    if (desc.type == TextureType::Texture3D) imageType = VK_IMAGE_TYPE_3D;
    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = imageType;
    imgInfo.format = ToVkFormat(desc.format);
    imgInfo.extent.width = desc.width;
    imgInfo.extent.height = desc.height;
    imgInfo.extent.depth = (desc.type == TextureType::Texture3D) ? desc.depthOrArrayLayers : 1;
    imgInfo.mipLevels = desc.mipLevels;
    imgInfo.arrayLayers = (desc.type == TextureType::Texture2DArray || desc.type == TextureType::TextureCube) ? desc.depthOrArrayLayers : 1;
    imgInfo.samples = static_cast<VkSampleCountFlagBits>(desc.samples);
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = ToVkImageUsageForFormat(desc.format, desc.usage);
    if (desc.type == TextureType::TextureCube)
        imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    if (desc.transient) {
        imgInfo.usage = (desc.format == RHITextureFormat::Depth24 || desc.format == RHITextureFormat::Depth24Stencil8)
            ? (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
            : (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
        allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
    }

    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkResult res = vmaCreateImage(allocator, &imgInfo, &allocCreateInfo, &image, &allocation, nullptr);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::CreateTexture: vmaCreateImage failed (0x%x)", res);
        return nullptr;
    }

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = ToVkImageViewType(desc.type);
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange.aspectMask = ToVkImageAspect(desc.format);
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = imgInfo.arrayLayers;
    VkImageView imageView = VK_NULL_HANDLE;
    res = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
    if (res != VK_SUCCESS) {
        vmaDestroyImage(allocator, image, allocation);
        core::Log::ErrorF("DeviceVK::CreateTexture: vkCreateImageView failed (0x%x)", res);
        return nullptr;
    }

    auto* t = new TextureVK(this);
    t->m_width = desc.width;
    t->m_height = desc.height;
    t->m_depthOrArray = desc.depthOrArrayLayers;
    t->m_type = desc.type;
    t->m_format = desc.format;
    t->m_image = image;
    t->m_imageView = imageView;
    t->m_allocation = allocation;
    return t;
}

void DeviceVK::DestroyTexture(ITexture* texture) {
    if (!texture) return;
    TextureVK* t = static_cast<TextureVK*>(texture);
    VkDevice device = static_cast<VkDevice>(m_device);
    if (device && t->m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, static_cast<VkImageView>(t->m_imageView), nullptr);
        t->m_imageView = nullptr;
    }
    VmaAllocator allocator = static_cast<VmaAllocator>(m_allocator);
    if (allocator && t->m_image != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, static_cast<VkImage>(t->m_image), static_cast<VmaAllocation>(t->m_allocation));
        t->m_image = nullptr;
        t->m_allocation = nullptr;
    }
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
    if (!fence || !m_device || !m_timelineSemaphore) return false;
    FenceVK* f = static_cast<FenceVK*>(fence);
    if (f->m_device != this) return false;
    VkSemaphore sem = static_cast<VkSemaphore>(m_timelineSemaphore);
    VkSemaphoreWaitInfo waitInfo = {};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &sem;
    waitInfo.pValues = &f->m_timelineValue;
    uint64_t timeoutNs = (timeoutMs == 0) ? 0 : (timeoutMs > (UINT64_MAX / 1000000) ? UINT64_MAX : timeoutMs * 1000000);
    VkResult res = vkWaitSemaphores(static_cast<VkDevice>(m_device), &waitInfo, timeoutNs);
    return (res == VK_SUCCESS);
}

void DeviceVK::DestroyFence(IFence* fence) { if (fence) delete static_cast<FenceVK*>(fence); }

IShader* DeviceVK::CreateShader(ShaderStage stage, const char* source, size_t sourceLen) {
    (void)source; (void)sourceLen;
    auto* s = new ShaderVK(this);
    s->m_stage = stage;
    return s;
}

IShader* DeviceVK::CreateShaderFromBinary(ShaderStage stage, const void* data, size_t size) {
    if (!m_device || !data || size < 4) return nullptr;
    const uint32_t spirvMagic = 0x07230203u;
    if (size >= 4 && *static_cast<const uint32_t*>(data) != spirvMagic) {
        core::Log::Error("DeviceVK::CreateShaderFromBinary: data is not SPIR-V (invalid magic)");
        return nullptr;
    }
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = static_cast<const uint32_t*>(data);
    VkShaderModule module = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(static_cast<VkDevice>(m_device), &createInfo, nullptr, &module);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::CreateShaderFromBinary: vkCreateShaderModule failed (0x%x)", res);
        return nullptr;
    }
    auto* s = new ShaderVK(this);
    s->m_stage = stage;
    s->m_module = module;
    return s;
}

static std::vector<uint8_t> ReadFileBinary(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    const size_t size = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint8_t> buf(size);
    if (!f.read(reinterpret_cast<char*>(buf.data()), size)) return {};
    return buf;
}

bool DeviceVK::TryLoadShaderFromPaths(const char* vertexPath, const char* fragmentPath, IShader*& outVs, IShader*& outFs) {
    outVs = nullptr;
    outFs = nullptr;
    if (!vertexPath || !fragmentPath) return false;
    std::vector<uint8_t> vsData = ReadFileBinary(vertexPath);
    std::vector<uint8_t> fsData = ReadFileBinary(fragmentPath);
    if (vsData.size() < 4 || fsData.size() < 4) return false;
    const uint32_t spirvMagic = 0x07230203u;
    if (*reinterpret_cast<uint32_t*>(vsData.data()) != spirvMagic || *reinterpret_cast<uint32_t*>(fsData.data()) != spirvMagic)
        return false;
    outVs = CreateShaderFromBinary(ShaderStage::Vertex, vsData.data(), vsData.size());
    outFs = CreateShaderFromBinary(ShaderStage::Fragment, fsData.data(), fsData.size());
    return (outVs && outFs && static_cast<ShaderVK*>(outVs)->IsValid() && static_cast<ShaderVK*>(outFs)->IsValid());
}

void DeviceVK::DestroyShader(IShader* shader) {
    if (!shader) return;
    ShaderVK* s = static_cast<ShaderVK*>(shader);
    if (s->m_module && m_device) {
        vkDestroyShaderModule(static_cast<VkDevice>(m_device), static_cast<VkShaderModule>(s->m_module), nullptr);
        s->m_module = nullptr;
    }
    delete s;
}

IPipeline* DeviceVK::CreateGraphicsPipeline(const PipelineDesc& desc) {
    const uint64_t hash = HashPipelineDesc(desc);
    void* pipeline = GetCachedGraphicsPipeline(hash);
    if (!pipeline && desc.vertexShader && desc.fragmentShader && m_defaultPipelineLayout && m_pipelineCache) {
        void* created = CreateVkGraphicsPipeline(this, desc, m_defaultPipelineLayout, m_pipelineCache);
        if (created) {
            RegisterGraphicsPipeline(hash, created);
            pipeline = created;
        }
    }

    auto* p = new PipelineVK(this);
    p->m_vertexLayout = desc.vertexLayout;
    p->m_topology = desc.topology;
    p->m_patchVertexCount = desc.patchVertexCount;
    p->m_blend = desc.blend;
    p->m_depthStencil = desc.depthStencil;
    p->m_raster = desc.raster;
    p->m_pipeline = pipeline;
    return p;
}

IPipeline* DeviceVK::CreateGraphicsPipelineFromName(const char* shaderName, const VertexLayout& layout, PrimitiveTopology topology, const DepthStencilState* depthStencilOverride) {
    if (!shaderName || !shaderName[0]) return nullptr;
    const std::string logicalName = ShaderPathToLogicalName(shaderName);
    if (logicalName.empty()) return nullptr;
    std::string root = platform::filesystem::GetEngineAssetsRoot();
    std::string baseDir = root.empty()
        ? std::string("assets/shaders/vulkan/")
        : platform::filesystem::NormalizePath(root + "/assets/shaders/vulkan/");
    std::string vertPath = baseDir + logicalName + ".vert.spv";
    std::string fragPath = baseDir + logicalName + ".frag.spv";

    rhi::IShader* vs = nullptr;
    rhi::IShader* fs = nullptr;
    if (!TryLoadShaderFromPaths(vertPath.c_str(), fragPath.c_str(), vs, fs) || !vs || !fs) {
        if (vs) DestroyShader(vs);
        if (fs) DestroyShader(fs);
        return nullptr;
    }
    PipelineDesc desc;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;
    desc.vertexLayout = layout;
    desc.topology = topology;
    if (depthStencilOverride)
        desc.depthStencil = *depthStencilOverride;
    else {
        desc.depthStencil.depthTest = true;
        desc.depthStencil.depthWrite = true;
    }
    IPipeline* pipe = CreateGraphicsPipeline(desc);
    DestroyShader(vs);
    DestroyShader(fs);
    return pipe;
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
    (void)computeShader;
    auto* p = new PipelineVK(this);
    p->m_isCompute = true;
    return p;
}

void DeviceVK::DestroyPipeline(IPipeline* pipeline) { if (pipeline) delete static_cast<PipelineVK*>(pipeline); }

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
    if (width == 0 || height == 0) return nullptr;
    auto* rt = new RenderTargetVK(this);
    rt->m_width = width;
    rt->m_height = height;
    TextureDesc texDesc;
    texDesc.width = width;
    texDesc.height = height;
    texDesc.mipLevels = 1;
    texDesc.samples = (samples > 0) ? samples : 1u;
    texDesc.usage = transient ? TextureUsage::RenderTarget : (TextureUsage::ShaderResource | TextureUsage::RenderTarget);
    texDesc.transient = transient;
    for (RHITextureFormat fmt : colorFormats) {
        texDesc.format = fmt;
        ITexture* tex = CreateTexture(texDesc, nullptr);
        if (!tex) { DestroyRenderTarget(rt); return nullptr; }
        rt->m_colorTextures.push_back(std::unique_ptr<TextureVK>(static_cast<TextureVK*>(tex)));
    }
    if (depthFormat != RHITextureFormat::None) {
        texDesc.format = depthFormat;
        texDesc.usage = transient ? TextureUsage::RenderTarget : (TextureUsage::ShaderResource | TextureUsage::RenderTarget);
        texDesc.transient = transient;
        ITexture* depthTex = CreateTexture(texDesc, nullptr);
        if (!depthTex) { DestroyRenderTarget(rt); return nullptr; }
        rt->m_depthTexture = std::unique_ptr<TextureVK>(static_cast<TextureVK*>(depthTex));
    }
    return rt;
}

IRenderTarget* DeviceVK::CreateDepthOnlyRenderTargetArray(uint32_t width, uint32_t height, uint32_t arrayLayers) {
    (void)width; (void)height; (void)arrayLayers;
    return nullptr;
}

IRenderTarget* DeviceVK::CreateRenderTargetWithSharedDepth(uint32_t width, uint32_t height,
                                                           const std::vector<RHITextureFormat>& colorFormats,
                                                           ITexture* sharedDepthTexture,
                                                           uint32_t samples) {
    (void)width; (void)height; (void)colorFormats; (void)sharedDepthTexture; (void)samples;
    return nullptr;
}

IRenderTarget* DeviceVK::CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) {
    (void)colorTexture; (void)face; (void)mipLevel;
    return nullptr;
}

IRenderTarget* DeviceVK::CreateRenderTargetFromTextureWithDepth(ITexture* colorTexture, uint32_t face, uint32_t mipLevel, ITexture* depthTexture) {
    (void)colorTexture; (void)face; (void)mipLevel; (void)depthTexture;
    return nullptr;
}

void DeviceVK::DestroyRenderTarget(IRenderTarget* rt) {
    if (!rt) return;
    RenderTargetVK* rtVK = static_cast<RenderTargetVK*>(rt);
    for (auto& t : rtVK->m_colorTextures) {
        DestroyTexture(t.release());
    }
    rtVK->m_colorTextures.clear();
    if (rtVK->m_depthTexture) {
        DestroyTexture(rtVK->m_depthTexture.release());
    }
    rtVK->m_externalColorTextures.clear();
    rtVK->m_externalDepthTexture = nullptr;
    delete rtVK;
}

ITexture* DeviceVK::CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) {
    (void)nativeTextureId; (void)width; (void)height; (void)format;
    return nullptr;
}

IQueryPool* DeviceVK::CreateQueryPool(QueryType type, uint32_t count) {
    auto* q = new QueryPoolVK(this);
    q->m_type = type;
    q->m_count = count;
    return q;
}

void DeviceVK::DestroyQueryPool(IQueryPool* pool) { if (pool) delete static_cast<QueryPoolVK*>(pool); }

IPipelineLayout* DeviceVK::CreatePipelineLayout(const PipelineLayoutDesc& desc) {
    VkDevice device = static_cast<VkDevice>(m_device);
    if (!device) return nullptr;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings(desc.bindings.size());
    for (size_t i = 0; i < desc.bindings.size(); ++i) {
        const BindingDesc& b = desc.bindings[i];
        layoutBindings[i].binding = b.binding;
        layoutBindings[i].descriptorType = ToVkDescriptorType(b.type);
        layoutBindings[i].descriptorCount = 1;
        layoutBindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings[i].pImmutableSamplers = nullptr;
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutInfo.pBindings = layoutBindings.data();

    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &setLayout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DeviceVK::CreatePipelineLayout: vkCreateDescriptorSetLayout failed (0x%x)", res);
        return nullptr;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &setLayout;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    res = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (res != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
        core::Log::ErrorF("DeviceVK::CreatePipelineLayout: vkCreatePipelineLayout failed (0x%x)", res);
        return nullptr;
    }

    auto* pl = new PipelineLayoutVK();
    pl->m_desc = desc;
    pl->m_descriptorSetLayout = setLayout;
    pl->m_pipelineLayout = pipelineLayout;
    return pl;
}

void DeviceVK::DestroyPipelineLayout(IPipelineLayout* layout) {
    if (!layout) return;
    PipelineLayoutVK* pl = static_cast<PipelineLayoutVK*>(layout);
    VkDevice device = static_cast<VkDevice>(m_device);
    if (device) {
        if (pl->m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, static_cast<VkPipelineLayout>(pl->m_pipelineLayout), nullptr);
            pl->m_pipelineLayout = nullptr;
        }
        if (pl->m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, static_cast<VkDescriptorSetLayout>(pl->m_descriptorSetLayout), nullptr);
            pl->m_descriptorSetLayout = nullptr;
        }
    }
    delete pl;
}

IDescriptorSet* DeviceVK::CreateDescriptorSet(IPipelineLayout* layout) {
    PipelineLayoutVK* pl = static_cast<PipelineLayoutVK*>(layout);
    if (!pl || !m_device || !m_descriptorPool || pl->m_descriptorSetLayout == VK_NULL_HANDLE) return nullptr;
    VkDescriptorSetLayout setLayout = static_cast<VkDescriptorSetLayout>(pl->m_descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = static_cast<VkDescriptorPool>(m_descriptorPool);
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &setLayout;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkResult res = vkAllocateDescriptorSets(static_cast<VkDevice>(m_device), &allocInfo, &descriptorSet);
    if (res != VK_SUCCESS) return nullptr;
    auto* ds = new DescriptorSetVK(this, pl);
    ds->m_descriptorSet = descriptorSet;
    return ds;
}

void DeviceVK::DestroyDescriptorSet(IDescriptorSet* set) {
    if (!set) return;
    DescriptorSetVK* ds = static_cast<DescriptorSetVK*>(set);
    VkDescriptorSet vkSet = static_cast<VkDescriptorSet>(ds->m_descriptorSet);
    if (vkSet != VK_NULL_HANDLE && m_device && m_descriptorPool)
        vkFreeDescriptorSets(static_cast<VkDevice>(m_device), static_cast<VkDescriptorPool>(m_descriptorPool), 1, &vkSet);
    ds->m_descriptorSet = nullptr;
    delete ds;
}

IBindlessHeap* DeviceVK::CreateBindlessHeap() { return new BindlessHeapVK(this); }

void DeviceVK::DestroyBindlessHeap(IBindlessHeap* heap) { if (heap) delete static_cast<BindlessHeapVK*>(heap); }

int DeviceVK::GetMaxSamples(int requestedSamples) { return std::min(requestedSamples, 4); }

void DeviceVK::Present() {
    if (!m_immediateCommandList || !m_graphicsQueue) return;
    CommandListVK* imm = m_immediateCommandList.get();
    if (imm->IsRecording())
        imm->End();
    VkDevice device = static_cast<VkDevice>(m_device);
    VkQueue queue = static_cast<VkQueue>(m_graphicsQueue);
    VkCommandBuffer buf = static_cast<VkCommandBuffer>(imm->GetCommandBuffer());
    if (buf != VK_NULL_HANDLE) {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buf;
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    }
    VkSwapchainKHR swapchain = static_cast<VkSwapchainKHR>(m_swapchain);
    if (swapchain != VK_NULL_HANDLE && m_currentSwapchainImageIndex != 0xFFFFFFFFu) {
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &m_currentSwapchainImageIndex;
        PFN_vkQueuePresentKHR pfnPresent = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(device, "vkQueuePresentKHR"));
        if (pfnPresent) pfnPresent(queue, &presentInfo);
        m_currentSwapchainImageIndex = 0xFFFFFFFFu;
    }
    vkQueueWaitIdle(queue);
}

void DeviceVK::Finish() {
    if (m_graphicsQueue)
        vkQueueWaitIdle(static_cast<VkQueue>(m_graphicsQueue));
}

void DeviceVK::BlitToDefaultFramebuffer(IRenderTarget* src) {
    if (!src || !m_swapchain || m_swapchainImages.empty() || !m_immediateCommandList) return;
    RenderTargetVK* rt = static_cast<RenderTargetVK*>(src);
    ITexture* color0 = rt->GetColorTexture(0);
    if (!color0) return;
    TextureVK* tex = static_cast<TextureVK*>(color0);
    VkImage srcImage = static_cast<VkImage>(tex->m_image);
    if (srcImage == VK_NULL_HANDLE) return;
    VkDevice device = static_cast<VkDevice>(m_device);
    VkSwapchainKHR swapchain = static_cast<VkSwapchainKHR>(m_swapchain);
    VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &m_currentSwapchainImageIndex);
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) return;
    if (m_currentSwapchainImageIndex >= m_swapchainImages.size()) return;
    CommandListVK* imm = m_immediateCommandList.get();
    imm->EndRenderingIfActive();
    VkCommandBuffer cmd = static_cast<VkCommandBuffer>(imm->GetCommandBuffer());
    if (!imm->IsRecording() || cmd == VK_NULL_HANDLE) return;
    VkImage dstImage = static_cast<VkImage>(m_swapchainImages[m_currentSwapchainImageIndex]);
    uint32_t srcW = tex->m_width;
    uint32_t srcH = tex->m_height;
    uint32_t w = std::min(srcW, m_swapchainWidth);
    uint32_t h = std::min(srcH, m_swapchainHeight);
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
    barrierDst.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierDst.dstAccessMask = 0;
    barrierDst.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierDst.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierDst);
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
