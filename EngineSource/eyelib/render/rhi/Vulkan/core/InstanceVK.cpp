#include "eyelib/render/rhi/Vulkan/core/InstanceVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstring>
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

namespace eyelib {
namespace render {
namespace rhi {

bool InstanceVK::Create(InstanceVK* out, const char* appName, uint32_t appVersion,
                        const char* engineName, uint32_t engineVersion, uint32_t apiVersion) {
    if (!out) return false;
    if (appVersion == 0) appVersion = VK_MAKE_VERSION(1, 0, 0);
    if (engineVersion == 0) engineVersion = VK_MAKE_VERSION(1, 0, 0);
    uint32_t loaderApiVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion(&loaderApiVersion) != VK_SUCCESS)
        loaderApiVersion = VK_API_VERSION_1_0;
    if (apiVersion == 0) apiVersion = VK_API_VERSION_1_3;
    if (apiVersion > loaderApiVersion) apiVersion = loaderApiVersion;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName;
    appInfo.applicationVersion = appVersion;
    appInfo.pEngineName = engineName;
    appInfo.engineVersion = engineVersion;
    appInfo.apiVersion = apiVersion;

    std::vector<const char*> instanceExtensions = { "VK_KHR_surface" };
#if defined(_WIN32)
    instanceExtensions.push_back("VK_KHR_win32_surface");
#endif

    std::vector<const char*> instanceLayers;
#if defined(OPENEYE_VULKAN_VALIDATION) && OPENEYE_VULKAN_VALIDATION
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    if (layerCount) vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    bool hasValidation = false;
    for (const auto& prop : availableLayers) {
        if (std::strcmp(prop.layerName, validationLayerName) == 0) {
            hasValidation = true;
            break;
        }
    }
    if (hasValidation) {
        instanceLayers.push_back(validationLayerName);
        instanceExtensions.push_back("VK_EXT_debug_utils");
        core::Log::Info("Vulkan: validation layer enabled (VK_LAYER_KHRONOS_validation).");
    } else {
        core::Log::Warn("Vulkan: OPENEYE_VULKAN_VALIDATION requested but VK_LAYER_KHRONOS_validation not found; skipping.");
    }
#endif

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
    instanceCreateInfo.ppEnabledLayerNames = instanceLayers.empty() ? nullptr : instanceLayers.data();

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("InstanceVK::Create: vkCreateInstance failed (0x%x)", res);
        return false;
    }
    out->handle = instance;
    out->apiVersion = apiVersion;
    return true;
}

void InstanceVK::Destroy(InstanceVK* inst) {
    if (!inst || !inst->handle) return;
    vkDestroyInstance(static_cast<VkInstance>(inst->handle), nullptr);
    inst->handle = nullptr;
    inst->apiVersion = 0;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
