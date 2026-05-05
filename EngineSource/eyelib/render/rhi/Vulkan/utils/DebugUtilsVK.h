#pragma once

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

/** Set GPU debug name for a Vulkan object when VK_EXT_debug_utils is available. No-op otherwise. */
void SetDebugNameBuffer(DeviceVK* device, void* vkBuffer, const char* name);
void SetDebugNameImage(DeviceVK* device, void* vkImage, const char* name);
void SetDebugNameImageView(DeviceVK* device, void* vkImageView, const char* name);
void SetDebugNamePipeline(DeviceVK* device, void* vkPipeline, const char* name);
void SetDebugNameDescriptorSet(DeviceVK* device, void* vkDescriptorSet, const char* name);

} // namespace rhi
} // namespace render
} // namespace eyelib
