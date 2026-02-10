#include "eyelib/render/rhi/bgfx/BGFXEnums.h"
#include <bgfx/bgfx.h>
#include <bgfx/defines.h>

namespace eyelib {
namespace render {
namespace rhi {
namespace BGFXEnums {

uint16_t TextureFormatToBGFX(RHITextureFormat format) {
    using F = bgfx::TextureFormat::Enum;
    switch (format) {
        case RHITextureFormat::R8: return F::R8;
        case RHITextureFormat::RG8: return F::RG8;
        case RHITextureFormat::RGB8: return F::RGB8;
        case RHITextureFormat::RGBA8: return F::RGBA8;
        case RHITextureFormat::RGB16F: return F::RGBA16F; // bgfx has no RGB16F, use RGBA16F
        case RHITextureFormat::RGBA16F: return F::RGBA16F;
        case RHITextureFormat::Depth24: return F::D24;
        case RHITextureFormat::Depth24Stencil8: return F::D24S8;
        case RHITextureFormat::None: return F::RGBA8;
        default: return F::RGBA8;
    }
}

uint64_t TextureFormatToBGFXSamplerFlags(RHITextureFormat format) {
    (void)format;
    return BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE;
}

uint64_t PrimitiveTopologyToBGFX(PrimitiveTopology topo) {
    switch (topo) {
        case PrimitiveTopology::TriangleList: return 0;
        case PrimitiveTopology::TriangleStrip: return BGFX_STATE_PT_TRISTRIP;
        case PrimitiveTopology::PointList: return BGFX_STATE_PT_POINTS;
        case PrimitiveTopology::LineList: return BGFX_STATE_PT_LINES;
        default: return 0;
    }
}

uint32_t IndexTypeToBGFX(IndexType type) {
    return type == IndexType::UInt32 ? BGFX_BUFFER_INDEX32 : 0;
}

uint64_t BlendStateToBGFX(const BlendState& blend) {
    if (!blend.blendEnable) return 0;
    return BGFX_STATE_BLEND_ALPHA;
}

uint64_t DepthStencilStateToBGFX(const DepthStencilState& depth) {
    uint64_t state = 0;
    if (depth.depthTest)
        state |= depth.depthCompareLess ? BGFX_STATE_DEPTH_TEST_LESS : BGFX_STATE_DEPTH_TEST_GEQUAL;
    if (depth.depthWrite) state |= BGFX_STATE_WRITE_Z;
    return state;
}

uint64_t RasterStateToBGFX(const RasterState& raster) {
    if (!raster.cullEnable) return 0;
    return raster.frontCounterClockwise ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW;
}

} // namespace BGFXEnums
} // namespace rhi
} // namespace render
} // namespace eyelib
