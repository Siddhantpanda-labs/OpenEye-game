#include "eyelib/render/rhi/OpenGL/GLEnums.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {
namespace rhi {
namespace GLEnums {

unsigned int BufferUsageToGL(BufferUsage usage) {
    unsigned int gl = 0;
    if (HasUsage(usage, BufferUsage::Vertex)) gl |= GL_MAP_READ_BIT;  // use as vertex
    if (HasUsage(usage, BufferUsage::Index)) gl |= GL_MAP_READ_BIT;
    if (HasUsage(usage, BufferUsage::Uniform)) gl |= GL_MAP_READ_BIT;
    if (HasUsage(usage, BufferUsage::TransferDst)) gl |= GL_MAP_WRITE_BIT;
    (void)gl;
    return GL_STATIC_DRAW;  // OpenGL hint; we use STATIC_DRAW for immutable, DYNAMIC_DRAW for uploads
}

unsigned int TextureTypeToGL(TextureType type) {
    switch (type) {
        case TextureType::Texture2D: return GL_TEXTURE_2D;
        case TextureType::Texture2DArray: return GL_TEXTURE_2D_ARRAY;
        case TextureType::TextureCube: return GL_TEXTURE_CUBE_MAP;
        case TextureType::Texture3D: return GL_TEXTURE_3D;
        case TextureType::TextureBuffer: return GL_TEXTURE_BUFFER;
        default: return GL_TEXTURE_2D;
    }
}

unsigned int TextureFormatToGLInternal(RHITextureFormat format) {
    switch (format) {
        case RHITextureFormat::R8: return GL_R8;
        case RHITextureFormat::RG8: return GL_RG8;
        case RHITextureFormat::RGB8: return GL_RGB8;
        case RHITextureFormat::RGBA8: return GL_RGBA8;
        case RHITextureFormat::RGB8_SRGB: return GL_SRGB8;
        case RHITextureFormat::RGBA8_SRGB: return GL_SRGB8_ALPHA8;
        case RHITextureFormat::BGRA8: return GL_RGBA8;
        case RHITextureFormat::RGB16F: return GL_RGB16F;
        case RHITextureFormat::RGBA16F: return GL_RGBA16F;
        case RHITextureFormat::R32UI: return GL_R32UI;
        case RHITextureFormat::Depth24: return GL_DEPTH_COMPONENT24;
        case RHITextureFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
        case RHITextureFormat::None: return GL_RGBA8;
        default: return GL_RGBA8;
    }
}

unsigned int TextureFormatToGLFormat(RHITextureFormat format) {
    switch (format) {
        case RHITextureFormat::R8: return GL_RED;
        case RHITextureFormat::RG8: return GL_RG;
        case RHITextureFormat::RGB8:
        case RHITextureFormat::RGB8_SRGB: return GL_RGB;
        case RHITextureFormat::RGBA8:
        case RHITextureFormat::RGBA8_SRGB: return GL_RGBA;
        case RHITextureFormat::BGRA8: return GL_BGRA;
        case RHITextureFormat::RGB16F: return GL_RGB;
        case RHITextureFormat::RGBA16F: return GL_RGBA;
        case RHITextureFormat::R32UI: return GL_RED_INTEGER;
        case RHITextureFormat::Depth24: return GL_DEPTH_COMPONENT;
        case RHITextureFormat::Depth24Stencil8: return GL_DEPTH_STENCIL;
        case RHITextureFormat::None: return GL_RGBA;
        default: return GL_RGBA;
    }
}

unsigned int TextureFormatToGLDataType(RHITextureFormat format) {
    switch (format) {
        case RHITextureFormat::R8:
        case RHITextureFormat::RG8:
        case RHITextureFormat::RGB8:
        case RHITextureFormat::RGBA8:
        case RHITextureFormat::RGB8_SRGB:
        case RHITextureFormat::RGBA8_SRGB:
            return GL_UNSIGNED_BYTE;
        case RHITextureFormat::RGB16F:
        case RHITextureFormat::RGBA16F:
            return GL_FLOAT;
        case RHITextureFormat::R32UI: return GL_UNSIGNED_INT;
        case RHITextureFormat::Depth24: return GL_UNSIGNED_INT;
        case RHITextureFormat::Depth24Stencil8: return GL_UNSIGNED_INT_24_8;
        default: return GL_UNSIGNED_BYTE;
    }
}

unsigned int ShaderStageToGL(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex: return GL_VERTEX_SHADER;
        case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
        case ShaderStage::Compute: return GL_COMPUTE_SHADER;
        case ShaderStage::TessControl: return GL_TESS_CONTROL_SHADER;
        case ShaderStage::TessEvaluation: return GL_TESS_EVALUATION_SHADER;
        case ShaderStage::Geometry: return GL_GEOMETRY_SHADER;
        default: return GL_VERTEX_SHADER;
    }
}

unsigned int PrimitiveTopologyToGL(PrimitiveTopology topo) {
    switch (topo) {
        case PrimitiveTopology::TriangleList: return GL_TRIANGLES;
        case PrimitiveTopology::TriangleStrip: return GL_TRIANGLE_STRIP;
        case PrimitiveTopology::PointList: return GL_POINTS;
        case PrimitiveTopology::LineList: return GL_LINES;
        case PrimitiveTopology::PatchList: return GL_PATCHES;
        default: return GL_TRIANGLES;
    }
}

unsigned int IndexTypeToGL(IndexType type) {
    return type == IndexType::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
}

unsigned int VertexAttributeTypeToGL(VertexAttributeType type) {
    switch (type) {
        case VertexAttributeType::Float:
        case VertexAttributeType::Float2:
        case VertexAttributeType::Float3:
        case VertexAttributeType::Float4:
            return GL_FLOAT;
        case VertexAttributeType::Int:
        case VertexAttributeType::Int2:
        case VertexAttributeType::Int3:
        case VertexAttributeType::Int4:
            return GL_INT;
        case VertexAttributeType::UInt4:
            return GL_UNSIGNED_INT;
        default: return GL_FLOAT;
    }
}

int VertexAttributeTypeCount(VertexAttributeType type) {
    switch (type) {
        case VertexAttributeType::Float: return 1;
        case VertexAttributeType::Float2: return 2;
        case VertexAttributeType::Float3: return 3;
        case VertexAttributeType::Float4: return 4;
        case VertexAttributeType::Int: return 1;
        case VertexAttributeType::Int2: return 2;
        case VertexAttributeType::Int3: return 3;
        case VertexAttributeType::Int4: return 4;
        case VertexAttributeType::UInt4: return 4;
        default: return 1;
    }
}

} // namespace GLEnums
} // namespace rhi
} // namespace render
} // namespace eyelib
