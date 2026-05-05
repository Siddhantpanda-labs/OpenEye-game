#include "eyelib/rendering/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include <vulkan/vulkan.h>
#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {

namespace {

using PFN_vkSetDebugUtilsObjectNameEXT = VkResult (VKAPI_PTR *)(VkDevice, const VkDebugUtilsObjectNameInfoEXT*);
static PFN_vkSetDebugUtilsObjectNameEXT s_pfnSetDebugUtilsObjectName = nullptr;
using PFN_vkCmdBeginDebugUtilsLabelEXT = void (VKAPI_PTR *)(VkCommandBuffer, const VkDebugUtilsLabelEXT*);
using PFN_vkCmdEndDebugUtilsLabelEXT = void (VKAPI_PTR *)(VkCommandBuffer);
static PFN_vkCmdBeginDebugUtilsLabelEXT s_pfnCmdBeginDebugUtilsLabel = nullptr;
static PFN_vkCmdEndDebugUtilsLabelEXT s_pfnCmdEndDebugUtilsLabel = nullptr;

static void EnsureObjectNameLoaded(VkDevice device) {
    if (s_pfnSetDebugUtilsObjectName != nullptr) return;
    if (!device) return;
    s_pfnSetDebugUtilsObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
}

static void EnsureCmdLabelLoaded(VkDevice device) {
    if (s_pfnCmdBeginDebugUtilsLabel != nullptr) return;
    if (!device) return;
    s_pfnCmdBeginDebugUtilsLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
    s_pfnCmdEndDebugUtilsLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
}

static void SetDebugName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* name) {
    if (!name || *name == '\0') return;
    EnsureObjectNameLoaded(device);
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

void SetDebugNameDevice(DeviceVK* device, const char* name) {
    if (!device || !device->GetDevice()) return;
    VkDevice dev = static_cast<VkDevice>(device->GetDevice());
    SetDebugName(dev, VK_OBJECT_TYPE_DEVICE, reinterpret_cast<uint64_t>(dev), name);
}

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

void SetDebugNameSwapchain(DeviceVK* device, void* vkSwapchainKHR, const char* name) {
    if (!device || !vkSwapchainKHR) return;
    VkDevice dev = static_cast<VkDevice>(device->GetDevice());
    SetDebugName(dev, VK_OBJECT_TYPE_SWAPCHAIN_KHR, reinterpret_cast<uint64_t>(vkSwapchainKHR), name);
}

void SetDebugNameAccelerationStructure(DeviceVK* device, void* vkAccelerationStructureKHR, const char* name) {
    if (!device || !vkAccelerationStructureKHR) return;
    VkDevice dev = static_cast<VkDevice>(device->GetDevice());
#if defined(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR)
    SetDebugName(dev, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, reinterpret_cast<uint64_t>(vkAccelerationStructureKHR), name);
#else
    (void)dev;
    (void)name;
#endif
}

void CmdDebugUtilsBeginLabel(DeviceVK* device, void* vkCommandBuffer, const char* name, const float colorRgba[4]) {
    if (!device || !vkCommandBuffer || !name || !colorRgba) return;
    VkDevice dev = static_cast<VkDevice>(device->GetDevice());
    EnsureCmdLabelLoaded(dev);
    if (!s_pfnCmdBeginDebugUtilsLabel) return;
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = colorRgba[0];
    label.color[1] = colorRgba[1];
    label.color[2] = colorRgba[2];
    label.color[3] = colorRgba[3];
    s_pfnCmdBeginDebugUtilsLabel(static_cast<VkCommandBuffer>(vkCommandBuffer), &label);
}

void CmdDebugUtilsEndLabel(DeviceVK* device, void* vkCommandBuffer) {
    if (!device || !vkCommandBuffer) return;
    VkDevice dev = static_cast<VkDevice>(device->GetDevice());
    EnsureCmdLabelLoaded(dev);
    if (!s_pfnCmdEndDebugUtilsLabel) return;
    s_pfnCmdEndDebugUtilsLabel(static_cast<VkCommandBuffer>(vkCommandBuffer));
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
