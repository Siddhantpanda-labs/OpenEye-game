#pragma once

#include "eyelib/render/rhi/RHITypes.h"

namespace eyelib {
namespace render {
namespace rhi {

/** Map RHI types to OpenGL enums. Only included by OpenGL backend .cpp files. */
namespace GLEnums {

unsigned int BufferUsageToGL(BufferUsage usage);
unsigned int TextureTypeToGL(TextureType type);
unsigned int TextureFormatToGLInternal(RHITextureFormat format);
unsigned int TextureFormatToGLFormat(RHITextureFormat format);
unsigned int TextureFormatToGLDataType(RHITextureFormat format);
unsigned int ShaderStageToGL(ShaderStage stage);
unsigned int PrimitiveTopologyToGL(PrimitiveTopology topo);
unsigned int IndexTypeToGL(IndexType type);
unsigned int VertexAttributeTypeToGL(VertexAttributeType type);
int VertexAttributeTypeCount(VertexAttributeType type);

} // namespace GLEnums
} // namespace rhi
} // namespace render
} // namespace eyelib
