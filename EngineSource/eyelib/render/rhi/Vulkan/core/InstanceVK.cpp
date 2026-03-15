#include "eyelib/render/rhi/Vulkan/core/InstanceVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <vector>
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
    if (apiVersion == 0) apiVersion = VK_API_VERSION_1_2;

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

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("InstanceVK::Create: vkCreateInstance failed (0x%x)", res);
        return false;
    }
    out->handle = instance;
    return true;
}

void InstanceVK::Destroy(InstanceVK* inst) {
    if (!inst || !inst->handle) return;
    vkDestroyInstance(static_cast<VkInstance>(inst->handle), nullptr);
    inst->handle = nullptr;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
