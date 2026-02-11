#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

struct PipelineBGFX : IPipeline {
    static constexpr uint16_t kInvalidIdx = 0xffff;
    uint16_t m_programIdx = kInvalidIdx;
    VertexLayout m_vertexLayout;
    PrimitiveTopology m_topology = PrimitiveTopology::TriangleList;
    BlendState m_blend;
    DepthStencilState m_depthStencil;
    RasterState m_raster;

    bool HasValidProgram() const override { return m_programIdx != kInvalidIdx; }
};

} // namespace rhi
} // namespace render
} // namespace eyelib
