#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct PipelineGL : IPipeline {
    unsigned int m_program = 0;  // GL program (vertex + fragment [+ tess + geometry] linked)
    VertexLayout m_vertexLayout;
    PrimitiveTopology m_topology = PrimitiveTopology::TriangleList;
    uint32_t m_patchVertexCount = 3;  // for tessellation (glPatchParameteri)
    bool m_hasTessellation = false;   // true if tess control + eval attached
    bool m_hasGeometry = false;       // true if geometry shader attached
    BlendState m_blend;
    DepthStencilState m_depthStencil;
    RasterState m_raster;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
