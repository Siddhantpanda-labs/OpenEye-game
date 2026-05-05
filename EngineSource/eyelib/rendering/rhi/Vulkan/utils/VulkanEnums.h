#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Map RHI buffer usage to VkBufferUsageFlags. */
inline VkBufferUsageFlags ToVkBufferUsage(BufferUsage usage) {
    VkBufferUsageFlags f = 0;
    if (HasUsage(usage, BufferUsage::Vertex)) f |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (HasUsage(usage, BufferUsage::Index)) f |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (HasUsage(usage, BufferUsage::Uniform)) f |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (HasUsage(usage, BufferUsage::Storage)) f |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (HasUsage(usage, BufferUsage::TransferSrc)) f |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (HasUsage(usage, BufferUsage::TransferDst)) f |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (HasUsage(usage, BufferUsage::IndirectDraw)) f |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (HasUsage(usage, BufferUsage::ShaderDeviceAddress)) f |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
#if defined(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR)
    if (HasUsage(usage, BufferUsage::AccelerationStructureStorage)) f |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
#endif
#if defined(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR)
    if (HasUsage(usage, BufferUsage::AccelerationStructureBuildInput)) f |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
#endif
#if defined(VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT)
    if (HasUsage(usage, BufferUsage::DescriptorBuffer)) f |= VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
#endif
    return f ? f : VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
}

/** Map RHI texture format to VkFormat. */
inline VkFormat ToVkFormat(RHITextureFormat format) {
    switch (format) {
        case RHITextureFormat::R8: return VK_FORMAT_R8_UNORM;
        case RHITextureFormat::RG8: return VK_FORMAT_R8G8_UNORM;
        // Prefer widely supported 4-channel formats on Vulkan for robust allocation/path handling.
        case RHITextureFormat::RGB8: return VK_FORMAT_R8G8B8A8_UNORM;
        case RHITextureFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
        case RHITextureFormat::RGB8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case RHITextureFormat::RGBA8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case RHITextureFormat::BGRA8: return VK_FORMAT_B8G8R8A8_UNORM;
        case RHITextureFormat::BGR10A2: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case RHITextureFormat::RGB16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case RHITextureFormat::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case RHITextureFormat::R32UI: return VK_FORMAT_R32_UINT;
        case RHITextureFormat::Depth24: return VK_FORMAT_D32_SFLOAT;
        case RHITextureFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
        case RHITextureFormat::None: return VK_FORMAT_UNDEFINED;
    }
    return VK_FORMAT_R8G8B8A8_UNORM;
}

/** Map RHI texture usage to VkImageUsageFlags. */
inline VkImageUsageFlags ToVkImageUsage(TextureUsage usage) {
    VkImageUsageFlags f = 0;
    if (HasUsage(usage, TextureUsage::ShaderResource)) f |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (HasUsage(usage, TextureUsage::RenderTarget)) f |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (HasUsage(usage, TextureUsage::TransferSrc)) f |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (HasUsage(usage, TextureUsage::TransferDst)) f |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return f ? f : VK_IMAGE_USAGE_SAMPLED_BIT;
}

inline VkImageUsageFlags ToVkImageUsageForFormat(RHITextureFormat format, TextureUsage usage) {
    const bool isDepth = (format == RHITextureFormat::Depth24 || format == RHITextureFormat::Depth24Stencil8);
    VkImageUsageFlags f = 0;
    if (HasUsage(usage, TextureUsage::ShaderResource)) f |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (HasUsage(usage, TextureUsage::TransferSrc)) f |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (HasUsage(usage, TextureUsage::TransferDst)) f |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (HasUsage(usage, TextureUsage::RenderTarget)) {
        if (isDepth) f |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        else f |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }
    return f ? f : (isDepth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_SAMPLED_BIT);
}

// ---- Resource state → Vulkan (barriers) ----

inline VkPipelineStageFlags ToVkPipelineStage(ResourceState state) {
    switch (state) {
        case ResourceState::Undefined:       return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case ResourceState::VertexBuffer:     return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        case ResourceState::IndexBuffer:      return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        case ResourceState::UniformBuffer:   return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case ResourceState::StorageBuffer:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case ResourceState::ShaderResource:   return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case ResourceState::RenderTarget:     return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case ResourceState::DepthWrite:      return VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        // Depth in READ_ONLY layout may be consumed as a read-only attachment *or* sampled in shaders (e.g. deferred).
        // Fragment-shader texture() reads require FRAGMENT_SHADER + SHADER_READ on the barrier dst (Vulkan sync2).
        case ResourceState::DepthRead:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case ResourceState::TransferDst:      return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case ResourceState::TransferSrc:     return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case ResourceState::Present:         return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        case ResourceState::IndirectDraw:    return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
#if defined(VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR)
        case ResourceState::AsBuildScratch:
        case ResourceState::AsBuildGeometryInput:
        case ResourceState::AsBuildStorage:
            return VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
#else
        case ResourceState::AsBuildScratch:
        case ResourceState::AsBuildGeometryInput:
        case ResourceState::AsBuildStorage:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
#endif
    }
    return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
}

inline VkAccessFlags ToVkAccessFlags(ResourceState state) {
    switch (state) {
        case ResourceState::Undefined:       return 0;
        case ResourceState::VertexBuffer:    return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        case ResourceState::IndexBuffer:     return VK_ACCESS_INDEX_READ_BIT;
        case ResourceState::UniformBuffer:   return VK_ACCESS_UNIFORM_READ_BIT;
        case ResourceState::StorageBuffer:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        case ResourceState::ShaderResource:  return VK_ACCESS_SHADER_READ_BIT;
        case ResourceState::RenderTarget:    return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case ResourceState::DepthWrite:      return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case ResourceState::DepthRead:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
        case ResourceState::TransferDst:     return VK_ACCESS_TRANSFER_WRITE_BIT;
        case ResourceState::TransferSrc:     return VK_ACCESS_TRANSFER_READ_BIT;
        case ResourceState::Present:        return 0;
        case ResourceState::IndirectDraw:    return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
#if defined(VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR)
        case ResourceState::AsBuildScratch:
            return VK_ACCESS_SHADER_STORAGE_READ_BIT | VK_ACCESS_SHADER_STORAGE_WRITE_BIT;
        case ResourceState::AsBuildGeometryInput:
            return VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        case ResourceState::AsBuildStorage:
            return VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
#else
        case ResourceState::AsBuildScratch:
        case ResourceState::AsBuildGeometryInput:
        case ResourceState::AsBuildStorage:
            return 0;
#endif
    }
    return 0;
}

inline VkPipelineStageFlags2 ToVkPipelineStage2(ResourceState state) {
    return static_cast<VkPipelineStageFlags2>(ToVkPipelineStage(state));
}

inline VkAccessFlags2 ToVkAccessFlags2(ResourceState state) {
    return static_cast<VkAccessFlags2>(ToVkAccessFlags(state));
}

inline VkImageLayout ToVkImageLayout(ResourceState state) {
    switch (state) {
        case ResourceState::Undefined:       return VK_IMAGE_LAYOUT_UNDEFINED;
        case ResourceState::ShaderResource:  return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ResourceState::RenderTarget:    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ResourceState::DepthWrite:      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ResourceState::DepthRead:       return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case ResourceState::TransferDst:     return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case ResourceState::TransferSrc:     return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ResourceState::Present:        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        default:                            return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

/** Image aspect for barriers and the default `VkImageView` on depth/stencil images (both planes where applicable). */
inline VkImageAspectFlags ToVkImageAspect(RHITextureFormat format) {
    if (format == RHITextureFormat::Depth24Stencil8)
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    if (format == RHITextureFormat::Depth24)
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

/** Aspect for a **sampled** `VkImageView` on combined depth/stencil formats (VUID-VkDescriptorImageInfo-imageView-01976). */
inline VkImageAspectFlags ToVkImageAspectForSampledDepthView(RHITextureFormat format) {
    if (format == RHITextureFormat::Depth24Stencil8 || format == RHITextureFormat::Depth24)
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    return ToVkImageAspect(format);
}

/** Image view type for descriptor sampling. */
inline VkImageViewType ToVkImageViewType(TextureType type) {
    switch (type) {
        case TextureType::Texture2D: return VK_IMAGE_VIEW_TYPE_2D;
        case TextureType::Texture2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureType::TextureCube: return VK_IMAGE_VIEW_TYPE_CUBE;
        case TextureType::Texture3D: return VK_IMAGE_VIEW_TYPE_3D;
        default: return VK_IMAGE_VIEW_TYPE_2D;
    }
}

/** BindingType to VkDescriptorType. */
inline VkDescriptorType ToVkDescriptorType(BindingType type) {
    switch (type) {
        case BindingType::Texture:
        case BindingType::TextureCube: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case BindingType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case BindingType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
}

// ---- Pipeline state (graphics) ----

inline VkPrimitiveTopology ToVkPrimitiveTopology(PrimitiveTopology t) {
    switch (t) {
        case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::PatchList: return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

inline VkFormat ToVkVertexFormat(VertexAttributeType t) {
    switch (t) {
        case VertexAttributeType::Float: return VK_FORMAT_R32_SFLOAT;
        case VertexAttributeType::Float2: return VK_FORMAT_R32G32_SFLOAT;
        case VertexAttributeType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexAttributeType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VertexAttributeType::Int: return VK_FORMAT_R32_SINT;
        case VertexAttributeType::Int2: return VK_FORMAT_R32G32_SINT;
        case VertexAttributeType::Int3: return VK_FORMAT_R32G32B32_SINT;
        case VertexAttributeType::Int4: return VK_FORMAT_R32G32B32A32_SINT;
        case VertexAttributeType::UInt4: return VK_FORMAT_R32G32B32A32_UINT;
    }
    return VK_FORMAT_R32G32B32_SFLOAT;
}

inline VkBlendFactor ToVkBlendFactor(BlendFactor f) {
    switch (f) {
        case BlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One: return VK_BLEND_FACTOR_ONE;
        case BlendFactor::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor: return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    }
    return VK_BLEND_FACTOR_SRC_ALPHA;
}

inline VkBlendOp ToVkBlendOp(BlendOp op) {
    switch (op) {
        case BlendOp::Add: return VK_BLEND_OP_ADD;
        case BlendOp::Subtract: return VK_BLEND_OP_SUBTRACT;
        case BlendOp::Min: return VK_BLEND_OP_MIN;
        case BlendOp::Max: return VK_BLEND_OP_MAX;
    }
    return VK_BLEND_OP_ADD;
}

inline VkCompareOp ToVkCompareOp(DepthCompare c) {
    switch (c) {
        case DepthCompare::Less: return VK_COMPARE_OP_LESS;
        case DepthCompare::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case DepthCompare::Greater: return VK_COMPARE_OP_GREATER;
    }
    return VK_COMPARE_OP_LESS;
}

inline VkIndexType ToVkIndexType(IndexType t) {
    return t == IndexType::UInt32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
