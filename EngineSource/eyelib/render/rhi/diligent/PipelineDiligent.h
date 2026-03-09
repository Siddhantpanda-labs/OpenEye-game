#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <string>

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
    /** When non-null, holds Diligent::IShaderResourceBinding* (owned; Release in dtor). Used for binding Constants/Material/Bones. */
    void* m_pSRB = nullptr;
    /** Logical shader name (e.g. "passes/post/tonemap") when created from name; used for slot->variable binding. */
    std::string m_shaderName;

    bool HasValidProgram() const override { return m_pNative != nullptr; }
};

} // namespace rhi
} // namespace render
} // namespace eyelib
