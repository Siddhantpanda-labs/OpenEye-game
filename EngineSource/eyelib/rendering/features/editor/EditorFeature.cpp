#include "eyelib/rendering/features/editor/EditorFeature.h"

#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"
#include "eyelib/rendering/shaders/ShaderPermutationKey.h"

namespace eyelib {
namespace rendering {

namespace {

const shaders::ShaderPermutationKey kPermEditorDebugLines{
    shaders::ShaderPermutationKey::ProgramId::EditorDebugLines, 1, 1};

}  // namespace

EditorFeature::~EditorFeature() {
    m_debugDrawPass.reset();
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
    m_pipelineDevice = nullptr;
}

rhi::ShaderGraphicsPipelineCreateInfo EditorFeature::MakeDebugLinesSpec() {
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    gp.assetPath = "debug/debug_lines";

    // Vertex layout: vec3 position @ location 0, vec4 colour @ location 1 — packed 28 bytes.
    rhi::VertexLayout layout{};
    layout.strideInBytes = 28;
    rhi::VertexAttribute pos{};
    pos.location = 0;
    pos.type = rhi::VertexAttributeType::Float3;
    pos.offsetInBytes = 0;
    rhi::VertexAttribute col{};
    col.location = 1;
    col.type = rhi::VertexAttributeType::Float4;
    col.offsetInBytes = 12;
    layout.attributes.push_back(pos);
    layout.attributes.push_back(col);
    gp.vertexLayout = layout;

    gp.topology = rhi::PrimitiveTopology::LineList;
    gp.layoutKind = rhi::PipelineLayoutKind::ComposeBootstrap;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;

    // Depth-tested overlay: read existing depth, never write.
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = true;
    gp.depthStencilOverride.depthWrite = false;
    gp.depthStencilOverride.depthCompare = rhi::DepthCompare::LessEqual;

    rhi::PipelineAttachmentFormats att{};
    att.color.push_back(rhi::RHITextureFormat::RGBA16F);
    att.depthStencil = rhi::RHITextureFormat::Depth24Stencil8;
    gp.attachmentFormats = att;
    return gp;
}

void EditorFeature::OnRegister(const RenderCapabilities& caps) {
    (void)caps;
    m_shaderLibrary.ClearRegistrations();
    m_shaderLibrary.AddEntry(kPermEditorDebugLines, MakeDebugLinesSpec());
}

void EditorFeature::OnUpload(FrameContext& frame, const RenderContext& render) {
    (void)frame;
    (void)render;
    // Line vertices are uploaded into the pass-private host-visible vertex buffer at Execute
    // time (the source is a tiny CPU-side array; staging through the global upload ring would
    // add a copy without benefit for an overlay that fits in a few KB).
}

void EditorFeature::OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) {
    if (m_pendingLines.empty())
        return;
    if (!render.device)
        return;

    m_shaderLibrary.WarmUpPipelines(render.device);
    m_pipelineDevice = render.device;

    rhi::IPipeline* pipeline = m_shaderLibrary.TryGetPipeline(kPermEditorDebugLines);
    if (!pipeline)
        return;

    if (!m_debugDrawPass)
        m_debugDrawPass = std::make_unique<DebugDrawPass>(&blackboard, render.device, pipeline);
    else
        m_debugDrawPass->Reconfigure(&blackboard, pipeline);

    m_debugDrawPass->SetLines(m_pendingLines.data(), m_pendingLines.size());
    graph.AddPassBorrowed(m_debugDrawPass.get());
}

void EditorFeature::OnShutdown() {
    m_debugDrawPass.reset();
    m_shaderLibrary.DestroyPipelines(m_pipelineDevice);
    m_shaderLibrary.ClearRegistrations();
    m_pipelineDevice = nullptr;
    m_pendingLines.clear();
}

void EditorFeature::SetLines(const std::vector<DebugLine>& lines) {
    m_pendingLines = lines;
}

}  // namespace rendering
}  // namespace eyelib
