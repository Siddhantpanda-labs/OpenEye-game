#pragma once

#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;
struct CommandListVK;

namespace vkhelpers {

bool CreateComputeSet0(VkDevice device, VkDescriptorPool descriptorPool,
                       VkDescriptorSetLayout* outLayout, VkDescriptorSet* outSet);

bool CreateComputePipelineLayout(VkDevice device, VkDescriptorSetLayout computeSet0Layout, VkDescriptorSetLayout bindlessSetLayout,
                                 VkPipelineLayout* outPipelineLayout);

void UpdateComputeSet0(DeviceVK* device, CommandListVK* cmd);

} // namespace vkhelpers

} // namespace rhi
} // namespace rendering
} // namespace eyelib
