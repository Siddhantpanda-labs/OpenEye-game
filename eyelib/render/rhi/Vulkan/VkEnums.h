#pragma once

#include "eyelib/render/rhi/RHITypes.h"
#include <vulkan/vulkan_core.h>

namespace eyelib {
namespace render {
namespace rhi {
namespace vk {

/** Map RHI types to Vulkan enums. Used only by Vulkan backend .cpp files. */
namespace VkEnums {

VkFormat Format(RHITextureFormat format);
VkImageType ImageType(TextureType type);
VkImageUsageFlags ImageUsage(TextureUsage usage);
VkBufferUsageFlags BufferUsageFlags(rhi::BufferUsage usage);
VkShaderStageFlagBits ShaderStage(rhi::ShaderStage stage);
VkPrimitiveTopology PrimitiveTopology(rhi::PrimitiveTopology topo);
VkIndexType IndexType(rhi::IndexType type);
VkFormat VertexFormat(rhi::VertexAttributeType type);
uint32_t VertexFormatSize(rhi::VertexAttributeType type);
VkCompareOp DepthCompare(bool depthCompareLess);
VkBlendFactor BlendFactor(rhi::BlendFactor f);
VkBlendOp BlendOp(rhi::BlendOp op);
VkCullModeFlags CullMode(bool cullFront);
VkFrontFace FrontFace(bool frontCounterClockwise);
VkImageLayout ImageLayout(rhi::ResourceState state);
VkAccessFlags AccessFlags(rhi::ResourceState state);
VkPipelineStageFlags PipelineStage(rhi::ResourceState state);

} // namespace VkEnums
} // namespace vk
} // namespace rhi
} // namespace render
} // namespace eyelib
