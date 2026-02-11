#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct PipelineDiligent : IPipeline {
    ~PipelineDiligent() override;
    VertexLayout m_vertexLayout;
    PrimitiveTopology m_topology = PrimitiveTopology::TriangleList;
    BlendState m_blend;
    DepthStencilState m_depthStencil;
    RasterState m_raster;
    /** When non-null, holds Diligent::IPipelineState* (owned; Release in dtor). Used when created from name. */
    void* m_pNative = nullptr;
    /** When non-null, holds Diligent::IShaderResourceBinding* (owned; Release in dtor). Created from PSO's PRS. */
    void* m_pSRB = nullptr;

    bool HasValidProgram() const override { return m_pNative != nullptr; }
};

} // namespace rhi
} // namespace render
} // namespace eyelib
