#pragma once

#include "eyelib/render/rhi/RHITypes.h"

namespace eyelib {
namespace render {
namespace rhi {

/** Map RHI types to bgfx. Only included by bgfx backend .cpp files. */
namespace BGFXEnums {

/** Returns bgfx::TextureFormat::Enum as uint16_t; cast in .cpp when calling bgfx API. */
uint16_t TextureFormatToBGFX(RHITextureFormat format);
/** BGFX_TEXTURE_* / BGFX_SAMPLER_* flags for createTexture*. */
uint64_t TextureFormatToBGFXSamplerFlags(RHITextureFormat format);
uint64_t PrimitiveTopologyToBGFX(PrimitiveTopology topo);
uint32_t IndexTypeToBGFX(IndexType type);
uint64_t BlendStateToBGFX(const BlendState& blend);
uint64_t DepthStencilStateToBGFX(const DepthStencilState& depth);
uint64_t RasterStateToBGFX(const RasterState& raster);

} // namespace BGFXEnums
} // namespace rhi
} // namespace render
} // namespace eyelib
