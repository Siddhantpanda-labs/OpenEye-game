#pragma once

#include "eyelib/render/rhi/RHITypes.h"

namespace eyelib {
namespace render {
namespace rhi {

/** Named attachment format sets for graphics pipelines (explicit Vulkan dynamic rendering; portable documentation). */
namespace StandardAttachmentFormats {

inline PipelineAttachmentFormats GBufferMRT() {
    return {{{RHITextureFormat::RGB16F, RHITextureFormat::RGB16F, RHITextureFormat::RGBA8, RHITextureFormat::RGBA8,
              RHITextureFormat::RGB16F}},
            RHITextureFormat::Depth24};
}

inline PipelineAttachmentFormats ForwardColorDepth() {
    return {{{RHITextureFormat::RGB16F}}, RHITextureFormat::Depth24};
}

inline PipelineAttachmentFormats ShadowDepthOnly() {
    return {{}, RHITextureFormat::Depth24};
}

inline PipelineAttachmentFormats DeferredLightingAccumulation() {
    return {{{RHITextureFormat::RGB16F}}, RHITextureFormat::Depth24};
}

inline PipelineAttachmentFormats PostHDR() {
    return {{{RHITextureFormat::RGB16F}}, RHITextureFormat::None};
}

inline PipelineAttachmentFormats PostHDRWithDepth() {
    return {{{RHITextureFormat::RGB16F}}, RHITextureFormat::Depth24};
}

inline PipelineAttachmentFormats PostLDR8() {
    return {{{RHITextureFormat::RGBA8}}, RHITextureFormat::None};
}

inline PipelineAttachmentFormats PostR8() {
    return {{{RHITextureFormat::R8}}, RHITextureFormat::None};
}

inline PipelineAttachmentFormats PostCopyBGRA() {
    return {{{RHITextureFormat::BGRA8}}, RHITextureFormat::None};
}

inline PipelineAttachmentFormats IBLFaceCapture() {
    return {{{RHITextureFormat::RGB16F}}, RHITextureFormat::None};
}

inline PipelineAttachmentFormats OutlinePostColor() {
    return {{{RHITextureFormat::RGB16F}}, RHITextureFormat::None};
}

inline PipelineAttachmentFormats EntityId() {
    return {{{RHITextureFormat::R32UI}}, RHITextureFormat::Depth24};
}

inline PipelineAttachmentFormats LegacyRGBA8() {
    return {{{RHITextureFormat::RGBA8}}, RHITextureFormat::None};
}

inline PipelineAttachmentFormats FluidRayDataRGBA16F() {
    return {{{RHITextureFormat::RGBA16F}}, RHITextureFormat::None};
}

inline PipelineAttachmentFormats BrdfLutRGBA16F() {
    return {{{RHITextureFormat::RGBA16F}}, RHITextureFormat::None};
}

} // namespace StandardAttachmentFormats

} // namespace rhi
} // namespace render
} // namespace eyelib
