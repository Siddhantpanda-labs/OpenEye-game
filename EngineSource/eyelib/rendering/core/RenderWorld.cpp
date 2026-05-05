#include "eyelib/rendering/core/RenderWorld.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphResourceNames.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/scheduler/RenderFeature.h"
#include "eyelib/core/Log.h"

#ifndef OPENEYE_RENDERING_GPU_STATS_LOG
#define OPENEYE_RENDERING_GPU_STATS_LOG 0
#endif

namespace eyelib {
namespace rendering {

namespace {

struct UploadRecordingPhaseScope {
    RenderResources& res;
    explicit UploadRecordingPhaseScope(RenderResources& r) : res(r) { res.BeginUploadRecordingPhase(); }
    ~UploadRecordingPhaseScope() { res.EndUploadRecordingPhase(); }
    UploadRecordingPhaseScope(const UploadRecordingPhaseScope&) = delete;
    UploadRecordingPhaseScope& operator=(const UploadRecordingPhaseScope&) = delete;
};

}  // namespace

RenderWorld::RenderWorld(rhi::IDevice* device) noexcept : m_device(device), m_scheduler(m_features) {}

bool RenderWorld::TrySubscribeFeature(IRenderFeature* feature) {
    if (!m_features.TryAdd(feature))
        return false;
    if (m_initialized) {
        feature->OnRegister(m_caps);
        feature->OnRegisterBlackboardWriters(m_blackboardWriters, m_caps);
    }
    return true;
}

bool RenderWorld::TryUnsubscribeFeature(IRenderFeature* feature) {
    if (!feature || !m_features.Contains(feature))
        return false;
    if (m_initialized) {
        feature->OnUnregisterBlackboardWriters(m_blackboardWriters);
        feature->OnShutdown();
    }
    return m_features.TryRemove(feature);
}

void RenderWorld::SetOutputSize(uint32_t width, uint32_t height) noexcept {
    m_render.outputWidth = width;
    m_render.outputHeight = height;
}

void RenderWorld::Initialize() {
    if (m_initialized || !m_device)
        return;

    m_blackboardWriters.Clear();
    (void)m_blackboardWriters.TryReserve(BlackboardSlot::MainHdrRenderTarget, writer_names::kRenderResources);
    (void)m_blackboardWriters.TryReserve(BlackboardSlot::GBufferRenderTarget, writer_names::kRenderResources);
    (void)m_blackboardWriters.TryReserve(BlackboardSlot::MainDepth, writer_names::kRenderResources);
    (void)m_blackboardWriters.TryReserve(BlackboardSlot::PerFrameUniformBuffer, writer_names::kRenderResources);
    (void)m_blackboardWriters.TryReserve(BlackboardSlot::PerViewUniformBuffer, writer_names::kRenderResources);
    (void)m_blackboardWriters.TryReserve(BlackboardSlot::PerObjectStorageBuffer, writer_names::kRenderResources);
    (void)m_blackboardWriters.TryReserve(BlackboardSlot::MaterialTableStorageBuffer, writer_names::kRenderResources);
    (void)m_blackboardWriters.TryReserve(BlackboardSlot::DirectionalLightUniformBuffer, writer_names::kRenderResources);
    m_blackboard.SetWriterRegistry(&m_blackboardWriters);

    m_caps.gpu = m_device->GetCapabilities();
    for (IRenderFeature* f : m_features.OrderedFeatures()) {
        if (f) {
            f->OnRegister(m_caps);
            f->OnRegisterBlackboardWriters(m_blackboardWriters, m_caps);
        }
    }
    m_initialized = true;
}

void RenderWorld::Shutdown() {
    if (!m_initialized)
        return;
    for (IRenderFeature* f : m_features.OrderedFeatures()) {
        if (f) {
            f->OnUnregisterBlackboardWriters(m_blackboardWriters);
            f->OnShutdown();
        }
    }
    m_features.Clear();
    m_blackboard.SetWriterRegistry(nullptr);
    m_blackboardWriters.Clear();
    m_gpuProfiler.Shutdown(m_device);
    m_resources.Shutdown(m_device);
    m_initialized = false;
}

void RenderWorld::PrepareUploadPhaseResources(RenderContext& render) {
    m_resources.EnsureAllocated(render.device, render.outputWidth, render.outputHeight);
    m_resources.EnsureUniformBuffers(render.device);
    m_resources.EnsureStagingUploadBuffer(render.device);
    m_resources.EnsureGpuUploadScratchBuffer(render.device);
    m_resources.EnsurePerObjectBuffer(render.device, m_drawList.VisibleCount());
    m_resources.EnsureMaterialTableBufferForDrawList(render.device, m_drawList);
    render.resources = &m_resources;

    m_blackboard.ClearFrame();
    if (rhi::IRenderTarget* hdr = m_resources.MainHdrRenderTarget())
        m_blackboard.PublishRenderTarget(BlackboardSlot::MainHdrRenderTarget, writer_names::kRenderResources, hdr);
    if (rhi::IRenderTarget* gbuffer = m_resources.GBufferRenderTarget())
        m_blackboard.PublishRenderTarget(BlackboardSlot::GBufferRenderTarget, writer_names::kRenderResources, gbuffer);
    if (rhi::ITexture* depth = m_resources.MainDepthTexture())
        m_blackboard.PublishTexture(BlackboardSlot::MainDepth, writer_names::kRenderResources, depth);
    if (rhi::IBuffer* perFrame = m_resources.PerFrameUniformBuffer())
        m_blackboard.PublishBuffer(BlackboardSlot::PerFrameUniformBuffer, writer_names::kRenderResources, perFrame);
    if (rhi::IBuffer* perView = m_resources.PerViewUniformBuffer())
        m_blackboard.PublishBuffer(BlackboardSlot::PerViewUniformBuffer, writer_names::kRenderResources, perView);
    if (rhi::IBuffer* perObj = m_resources.PerObjectStorageBuffer())
        m_blackboard.PublishBuffer(BlackboardSlot::PerObjectStorageBuffer, writer_names::kRenderResources, perObj);
    if (rhi::IBuffer* matTable = m_resources.MaterialTableStorageBuffer())
        m_blackboard.PublishBuffer(BlackboardSlot::MaterialTableStorageBuffer, writer_names::kRenderResources, matTable);
    if (rhi::IBuffer* dir = m_resources.DirectionalLightUniformBuffer())
        m_blackboard.PublishBuffer(BlackboardSlot::DirectionalLightUniformBuffer, writer_names::kRenderResources, dir);
}

void RenderWorld::RecordFeatureUploads(FrameContext& frame, RenderContext& render) {
    UploadRecordingPhaseScope uploadScope(m_resources);
    for (IRenderFeature* f : m_features.OrderedFeatures()) {
        if (f)
            f->OnUpload(frame, render);
    }
}

void RenderWorld::UploadPhase(FrameContext& frame, RenderContext& render, rhi::ICommandList* recordingCommandList) {
    render.drawList = &m_drawList;
    render.recordingCommandList = recordingCommandList;
    m_visibility.BuildCpuDrawList(render.sceneView, m_drawList);

    PrepareUploadPhaseResources(render);
    RecordFeatureUploads(frame, render);
}

void RenderWorld::RunFrame(float deltaSeconds, rhi::ICommandList* recordingCommandList) {
    if (!m_initialized)
        return;

    ++m_frame.frameIndex;
    m_frame.deltaSeconds = deltaSeconds;
    m_render.device = m_device;
    m_render.sceneView = m_sceneView;

    UploadPhase(m_frame, m_render, recordingCommandList);

    m_scheduler.Schedule(m_frame, m_render, m_caps, m_graph, m_blackboard);
    m_graph.Compile(m_device, &m_blackboard);
    m_gpuProfiler.BeginFrame(m_device, recordingCommandList);
    m_graph.Execute(m_device, recordingCommandList, &m_blackboard);
    m_gpuProfiler.EndFrame(recordingCommandList);
    m_stats.CaptureFromProfiler(m_frame.frameIndex, m_gpuProfiler);
#if OPENEYE_RENDERING_GPU_STATS_LOG
    if ((m_frame.frameIndex % 120u) == 0u) {
        core::Log::Info(m_stats.FormatPipelineSummary().c_str());
    }
#endif

    m_render.recordingCommandList = nullptr;
    m_render.resources = nullptr;
}

}  // namespace rendering
}  // namespace eyelib
