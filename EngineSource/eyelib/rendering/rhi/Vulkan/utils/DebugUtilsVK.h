#pragma once

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

/** **`VK_EXT_debug_utils`** helpers: names show in **RenderDoc**, **Nsight**, and **validation** messages when the instance
 *  enables **`VK_EXT_debug_utils`**. Functions resolve **`vkSetDebugUtilsObjectNameEXT`** / **`vkCmdBeginDebugUtilsLabelEXT`**
 *  / **`vkCmdEndDebugUtilsLabelEXT`** via **`vkGetDeviceProcAddr`** once per process; **no-op** if the extension is absent.
 *
 * **Handles:** non-dispatchable handles are cast to **`uint64_t`** as required by the API; dispatchable (**`VkDevice`**, etc.)
 *  use the same cast pattern as the validation layers. */
void SetDebugNameDevice(DeviceVK* device, const char* name);
void SetDebugNameBuffer(DeviceVK* device, void* vkBuffer, const char* name);
void SetDebugNameImage(DeviceVK* device, void* vkImage, const char* name);
void SetDebugNameImageView(DeviceVK* device, void* vkImageView, const char* name);
void SetDebugNamePipeline(DeviceVK* device, void* vkPipeline, const char* name);
void SetDebugNameDescriptorSet(DeviceVK* device, void* vkDescriptorSet, const char* name);
void SetDebugNameSwapchain(DeviceVK* device, void* vkSwapchainKHR, const char* name);

/** Nested **region** in a **`VkCommandBuffer`** (visible in captures / some GPU profilers). Begin and End calls must be paired. */
void CmdDebugUtilsBeginLabel(DeviceVK* device, void* vkCommandBuffer, const char* name, const float colorRgba[4]);
void CmdDebugUtilsEndLabel(DeviceVK* device, void* vkCommandBuffer);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
