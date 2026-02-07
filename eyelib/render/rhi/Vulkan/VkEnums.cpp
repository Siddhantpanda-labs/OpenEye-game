#include "eyelib/render/rhi/Vulkan/VkEnums.h"

namespace eyelib {
namespace render {
namespace rhi {
namespace vk {
namespace VkEnums {

VkFormat Format(RHITextureFormat format) {
    switch (format) {
        case RHITextureFormat::R8: return VK_FORMAT_R8_UNORM;
        case RHITextureFormat::RG8: return VK_FORMAT_R8G8_UNORM;
        case RHITextureFormat::RGB8: return VK_FORMAT_R8G8B8_UNORM;
        case RHITextureFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
        case RHITextureFormat::RGB16F: return VK_FORMAT_R16G16B16_SFLOAT;
        case RHITextureFormat::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case RHITextureFormat::Depth24: return VK_FORMAT_D32_SFLOAT;
        case RHITextureFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
        case RHITextureFormat::None: return VK_FORMAT_R8G8B8A8_UNORM;
        default: return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

VkImageType ImageType(TextureType type) {
    switch (type) {
        case TextureType::Texture2D:
        case TextureType::Texture2DArray:
        case TextureType::TextureCube: return VK_IMAGE_TYPE_2D;
        case TextureType::Texture3D: return VK_IMAGE_TYPE_3D;
        default: return VK_IMAGE_TYPE_2D;
    }
}

VkImageUsageFlags ImageUsage(TextureUsage usage) {
    VkImageUsageFlags f = 0;
    if (HasUsage(usage, TextureUsage::ShaderResource)) f |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (HasUsage(usage, TextureUsage::RenderTarget)) f |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (HasUsage(usage, TextureUsage::TransferSrc)) f |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (HasUsage(usage, TextureUsage::TransferDst)) f |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return f;
}

VkBufferUsageFlags BufferUsageFlags(rhi::BufferUsage usage) {
    VkBufferUsageFlags f = 0;
    if (HasUsage(usage, rhi::BufferUsage::Vertex)) f |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (HasUsage(usage, rhi::BufferUsage::Index)) f |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (HasUsage(usage, rhi::BufferUsage::Uniform)) f |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (HasUsage(usage, rhi::BufferUsage::Storage)) f |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (HasUsage(usage, rhi::BufferUsage::TransferSrc)) f |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (HasUsage(usage, rhi::BufferUsage::TransferDst)) f |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return f;
}

VkShaderStageFlagBits ShaderStage(rhi::ShaderStage stage) {
    switch (stage) {
        case rhi::ShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
        case rhi::ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case rhi::ShaderStage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
        default: return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

VkPrimitiveTopology PrimitiveTopology(rhi::PrimitiveTopology topo) {
    switch (topo) {
        case rhi::PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case rhi::PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case rhi::PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case rhi::PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

VkIndexType IndexType(rhi::IndexType type) {
    return type == rhi::IndexType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
}

VkFormat VertexFormat(rhi::VertexAttributeType type) {
    switch (type) {
        case rhi::VertexAttributeType::Float: return VK_FORMAT_R32_SFLOAT;
        case rhi::VertexAttributeType::Float2: return VK_FORMAT_R32G32_SFLOAT;
        case rhi::VertexAttributeType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
        case rhi::VertexAttributeType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case rhi::VertexAttributeType::Int: return VK_FORMAT_R32_SINT;
        case rhi::VertexAttributeType::Int2: return VK_FORMAT_R32G32_SINT;
        case rhi::VertexAttributeType::Int3: return VK_FORMAT_R32G32B32_SINT;
        case rhi::VertexAttributeType::Int4: return VK_FORMAT_R32G32B32A32_SINT;
        case rhi::VertexAttributeType::UInt4: return VK_FORMAT_R32G32B32A32_UINT;
        default: return VK_FORMAT_R32G32B32_SFLOAT;
    }
}

uint32_t VertexFormatSize(rhi::VertexAttributeType type) {
    switch (type) {
        case rhi::VertexAttributeType::Float: return 4;
        case rhi::VertexAttributeType::Float2: return 8;
        case rhi::VertexAttributeType::Float3: return 12;
        case rhi::VertexAttributeType::Float4: return 16;
        case rhi::VertexAttributeType::Int: return 4;
        case rhi::VertexAttributeType::Int2: return 8;
        case rhi::VertexAttributeType::Int3: return 12;
        case rhi::VertexAttributeType::Int4: return 16;
        case rhi::VertexAttributeType::UInt4: return 16;
        default: return 12;
    }
}

VkCompareOp DepthCompare(bool depthCompareLess) {
    return depthCompareLess ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_GREATER;
}

VkBlendFactor BlendFactor(rhi::BlendFactor f) {
    switch (f) {
        case rhi::BlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
        case rhi::BlendFactor::One: return VK_BLEND_FACTOR_ONE;
        case rhi::BlendFactor::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
        case rhi::BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case rhi::BlendFactor::DstAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
        case rhi::BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case rhi::BlendFactor::SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
        case rhi::BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case rhi::BlendFactor::DstColor: return VK_BLEND_FACTOR_DST_COLOR;
        case rhi::BlendFactor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        default: return VK_BLEND_FACTOR_SRC_ALPHA;
    }
}

VkBlendOp BlendOp(rhi::BlendOp op) {
    switch (op) {
        case rhi::BlendOp::Add: return VK_BLEND_OP_ADD;
        case rhi::BlendOp::Subtract: return VK_BLEND_OP_SUBTRACT;
        case rhi::BlendOp::Min: return VK_BLEND_OP_MIN;
        case rhi::BlendOp::Max: return VK_BLEND_OP_MAX;
        default: return VK_BLEND_OP_ADD;
    }
}

VkCullModeFlags CullMode(bool cullFront) {
    return cullFront ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
}

VkFrontFace FrontFace(bool frontCounterClockwise) {
    return frontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
}

VkImageLayout ImageLayout(rhi::ResourceState state) {
    switch (state) {
        case rhi::ResourceState::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
        case rhi::ResourceState::ShaderResource: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case rhi::ResourceState::RenderTarget: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case rhi::ResourceState::DepthWrite:
        case rhi::ResourceState::DepthRead: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case rhi::ResourceState::TransferDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case rhi::ResourceState::TransferSrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case rhi::ResourceState::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        default: return VK_IMAGE_LAYOUT_GENERAL;
    }
}

VkAccessFlags AccessFlags(rhi::ResourceState state) {
    switch (state) {
        case rhi::ResourceState::VertexBuffer:
        case rhi::ResourceState::IndexBuffer: return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        case rhi::ResourceState::UniformBuffer: return VK_ACCESS_UNIFORM_READ_BIT;
        case rhi::ResourceState::ShaderResource: return VK_ACCESS_SHADER_READ_BIT;
        case rhi::ResourceState::RenderTarget: return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case rhi::ResourceState::DepthWrite:
        case rhi::ResourceState::DepthRead: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case rhi::ResourceState::TransferDst: return VK_ACCESS_TRANSFER_WRITE_BIT;
        case rhi::ResourceState::TransferSrc: return VK_ACCESS_TRANSFER_READ_BIT;
        case rhi::ResourceState::Present: return 0;
        default: return 0;
    }
}

VkPipelineStageFlags PipelineStage(rhi::ResourceState state) {
    switch (state) {
        case rhi::ResourceState::VertexBuffer:
        case rhi::ResourceState::IndexBuffer: return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        case rhi::ResourceState::UniformBuffer:
        case rhi::ResourceState::ShaderResource: return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        case rhi::ResourceState::RenderTarget: return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case rhi::ResourceState::DepthWrite:
        case rhi::ResourceState::DepthRead: return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case rhi::ResourceState::TransferDst:
        case rhi::ResourceState::TransferSrc: return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case rhi::ResourceState::Present: return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        default: return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

} // namespace VkEnums
} // namespace vk
} // namespace rhi
} // namespace render
} // namespace eyelib
