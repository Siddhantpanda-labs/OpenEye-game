#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct PipelineGL : IPipeline {
    unsigned int m_program = 0;  // GL program (vertex + fragment linked)
    VertexLayout m_vertexLayout;
    PrimitiveTopology m_topology = PrimitiveTopology::TriangleList;
    BlendState m_blend;
    DepthStencilState m_depthStencil;
    RasterState m_raster;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
