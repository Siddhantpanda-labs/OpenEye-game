#include "eyelib/render/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include <vulkan/vulkan.h>
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

namespace {

using PFN_vkSetDebugUtilsObjectNameEXT = VkResult (VKAPI_PTR *)(VkDevice, const VkDebugUtilsObjectNameInfoEXT*);
static PFN_vkSetDebugUtilsObjectNameEXT s_pfnSetDebugUtilsObjectName = nullptr;

static void EnsureLoaded(VkDevice device) {
    if (s_pfnSetDebugUtilsObjectName != nullptr) return;
    if (!device) return;
    s_pfnSetDebugUtilsObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
}

static void SetDebugName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* name) {
    if (!name || *name == '\0') return;
    EnsureLoaded(device);
    if (!s_pfnSetDebugUtilsObjectName) return;
    VkDebugUtilsObjectNameInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.pNext = nullptr;
    info.objectType = objectType;
    info.objectHandle = objectHandle;
    info.pObjectName = name;
    s_pfnSetDebugUtilsObjectName(device, &info);
}

} // namespace

void SetDebugNameBuffer(DeviceVK* device, void* vkBuffer, const char* name) {
    if (!device || !vkBuffer) return;
    VkDevice dev = static_cast<VkDevice>(device->GetDevice());
    SetDebugName(dev, VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(vkBuffer), name);
}

void SetDebugNameImage(DeviceVK* device, void* vkImage, const char* name) {
    if (!device || !vkImage) return;
    VkDevice dev = static_cast<VkDevice>(device->GetDevice());
    SetDebugName(dev, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(vkImage), name);
}

void SetDebugNameImageView(DeviceVK* device, void* vkImageView, const char* name) {
    if (!device || !vkImageView) return;
    VkDevice dev = static_cast<VkDevice>(device->GetDevice());
    SetDebugName(dev, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(vkImageView), name);
}

void SetDebugNamePipeline(DeviceVK* device, void* vkPipeline, const char* name) {
    if (!device || !vkPipeline) return;
    VkDevice dev = static_cast<VkDevice>(device->GetDevice());
    SetDebugName(dev, VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(vkPipeline), name);
}

void SetDebugNameDescriptorSet(DeviceVK* device, void* vkDescriptorSet, const char* name) {
    if (!device || !vkDescriptorSet) return;
    VkDevice dev = static_cast<VkDevice>(device->GetDevice());
    SetDebugName(dev, VK_OBJECT_TYPE_DESCRIPTOR_SET, reinterpret_cast<uint64_t>(vkDescriptorSet), name);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
