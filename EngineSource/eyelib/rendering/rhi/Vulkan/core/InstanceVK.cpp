#include "eyelib/rendering/rhi/Vulkan/core/InstanceVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstring>
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

namespace eyelib {
namespace rendering {
namespace rhi {

#if defined(OPENEYE_VULKAN_VALIDATION) && OPENEYE_VULKAN_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL OpenEyeVulkanDebugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*userData*/) {
    (void)types;
    const char* msg = (data && data->pMessage) ? data->pMessage : "(no message)";
    const char* id = (data && data->pMessageIdName) ? data->pMessageIdName : "";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        core::Log::ErrorF("Vulkan validation [ERROR] %s: %s", id, msg);
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        core::Log::WarnF("Vulkan validation [WARN] %s: %s", id, msg);
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        core::Log::InfoF("Vulkan validation [INFO] %s: %s", id, msg);
    return VK_FALSE;
}
#endif

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

#if defined(OPENEYE_VULKAN_VALIDATION) && OPENEYE_VULKAN_VALIDATION
    // Route validation messages to core::Log (otherwise they are silently dropped without a messenger).
    if (hasValidation) {
        auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (createFn) {
            VkDebugUtilsMessengerCreateInfoEXT dbgInfo = {};
            dbgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            dbgInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            dbgInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dbgInfo.pfnUserCallback = OpenEyeVulkanDebugUtilsCallback;
            VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
            VkResult dres = createFn(instance, &dbgInfo, nullptr, &messenger);
            if (dres == VK_SUCCESS) {
                out->debugMessenger = messenger;
                core::Log::Info("Vulkan: debug utils messenger installed (validation → core::Log).");
            } else {
                core::Log::WarnF("Vulkan: vkCreateDebugUtilsMessengerEXT failed (0x%x); validation output will be suppressed.", dres);
            }
        } else {
            core::Log::Warn("Vulkan: vkCreateDebugUtilsMessengerEXT not available; validation output will be suppressed.");
        }
    }
#endif

    return true;
}

void InstanceVK::Destroy(InstanceVK* inst) {
    if (!inst || !inst->handle) return;
#if defined(OPENEYE_VULKAN_VALIDATION) && OPENEYE_VULKAN_VALIDATION
    if (inst->debugMessenger) {
        auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(static_cast<VkInstance>(inst->handle), "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFn)
            destroyFn(static_cast<VkInstance>(inst->handle),
                      static_cast<VkDebugUtilsMessengerEXT>(inst->debugMessenger), nullptr);
        inst->debugMessenger = nullptr;
    }
#endif
    vkDestroyInstance(static_cast<VkInstance>(inst->handle), nullptr);
    inst->handle = nullptr;
    inst->apiVersion = 0;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
