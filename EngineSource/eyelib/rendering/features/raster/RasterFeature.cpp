#include "eyelib/rendering/features/raster/RasterFeature.h"

#include "eyelib/rendering/core/FrameContext.h"
#include "eyelib/rendering/core/RenderContext.h"
#include "eyelib/rendering/core/RenderResources.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/framegraph/passes/BlitMainHdrToDefaultPass.h"
#include "eyelib/rendering/framegraph/passes/ClearBlackboardRenderTargetPass.h"
#include "eyelib/rendering/framegraph/passes/FullscreenHdrComposePass.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

#include <cstdint>
#include <cstring>

namespace eyelib {
namespace rendering {

namespace {

constexpr const char* kComposeVertGlsl = R"(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
layout(location = 0) out vec2 vUv;
void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    vUv = texCoord;
}
)";

constexpr const char* kComposeFragGlsl = R"(#version 450
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;
layout(push_constant) uniform Push { vec4 tint; } pc;
void main() {
    outColor = vec4(mix(pc.tint.rgb * 0.2, pc.tint.rgb, vUv.x), 1.0);
}
)";

}  // namespace

RasterFeature::~RasterFeature() {
    releaseComposePipeline();
}

void RasterFeature::releaseComposePipeline() noexcept {
    if (m_composePipeline && m_deviceForPipeline) {
        m_deviceForPipeline->DestroyPipeline(m_composePipeline);
        m_composePipeline = nullptr;
        m_deviceForPipeline = nullptr;
    }
}

rhi::VertexLayout RasterFeature::fullscreenVertexLayout() {
    using namespace rhi;
    constexpr uint32_t stride = 4u * sizeof(float);
    VertexLayout layout{};
    layout.strideInBytes = stride;
    layout.attributes = {
        {0, VertexAttributeType::Float2, 0u},
        {1, VertexAttributeType::Float2, 2u * sizeof(float)},
    };
    return layout;
}

void RasterFeature::ensureComposePipeline(rhi::IDevice* device) {
    if (!device || m_composePipeline)
        return;

    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    gp.assetPath = "internal/raster_compose_bootstrap";
    gp.vertexLayout = fullscreenVertexLayout();
    gp.topology = rhi::PrimitiveTopology::TriangleList;
    gp.layoutKind = rhi::PipelineLayoutKind::PushOnly;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::CompileFromSource;
    gp.vertexSourceOverride = kComposeVertGlsl;
    gp.fragmentSourceOverride = kComposeFragGlsl;
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = false;
    gp.depthStencilOverride.depthWrite = false;
    rhi::PipelineAttachmentFormats att{};
    att.color.push_back(rhi::RHITextureFormat::RGBA16F);
    att.depthStencil = rhi::RHITextureFormat::Depth24Stencil8;
    gp.attachmentFormats = att;

    m_composePipeline = device->CreateGraphicsPipelineFromShaderAsset(gp);
    m_deviceForPipeline = m_composePipeline ? device : nullptr;
}

void RasterFeature::OnShutdown() {
    releaseComposePipeline();
}

void RasterFeature::OnUpload(FrameContext& frame, const RenderContext& render) {
    RenderResources* res = render.resources;
    if (!res || !render.recordingCommandList)
        return;

    constexpr size_t kBytes = 16;
    alignas(8) uint8_t blob[kBytes] = {};
    std::memcpy(blob, &frame.frameIndex, sizeof(frame.frameIndex));
    constexpr uint32_t kMagic = 0x4f504545u;
    std::memcpy(blob + 8, &kMagic, sizeof(kMagic));

    (void)res->RecordGpuUploadScratchFromStaging(render.recordingCommandList, blob, kBytes);
}

void RasterFeature::OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) {
    ensureComposePipeline(render.device);

    rhi::ClearValue clear{};
    clear.color[0] = 0.12f;
    clear.color[1] = 0.14f;
    clear.color[2] = 0.20f;
    clear.color[3] = 1.0f;
    clear.depth = 1.0f;
    graph.AddPass(std::make_unique<ClearBlackboardRenderTargetPass>(&blackboard, BlackboardSlot::MainHdrRenderTarget, clear));

    if (m_composePipeline)
        graph.AddPass(std::make_unique<FullscreenHdrComposePass>(&blackboard, BlackboardSlot::MainHdrRenderTarget, m_composePipeline));

    graph.AddPass(std::make_unique<BlitMainHdrToDefaultPass>(render.device, &blackboard));
}

}  // namespace rendering
}  // namespace eyelib
