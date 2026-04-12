#include "eyelib/rendering/core/RenderWorld.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/scheduler/RenderFeature.h"

namespace eyelib {
namespace rendering {

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
    (void)m_blackboardWriters.TryReserve(BlackboardSlot::MainHdrRenderTarget, "RenderResources");
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
    m_resources.Shutdown(m_device);
    m_initialized = false;
}

void RenderWorld::UploadPhase(FrameContext& frame, RenderContext& render, rhi::ICommandList* recordingCommandList) {
    render.drawList = &m_drawList;
    render.recordingCommandList = recordingCommandList;
    m_visibility.BuildCpuDrawList(render.sceneView, m_drawList);

    m_resources.EnsureAllocated(render.device, render.outputWidth, render.outputHeight);
    m_resources.EnsureStagingUploadBuffer(render.device);
    m_resources.EnsureGpuUploadScratchBuffer(render.device);
    render.resources = &m_resources;

    m_blackboard.ClearFrame();
    if (rhi::IRenderTarget* hdr = m_resources.MainHdrRenderTarget())
        m_blackboard.PublishRenderTarget(BlackboardSlot::MainHdrRenderTarget, "RenderResources", hdr);

    for (IRenderFeature* f : m_features.OrderedFeatures()) {
        if (f)
            f->OnUpload(frame, render);
    }
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
    m_graph.Compile(m_device);
    m_graph.Execute(m_device, recordingCommandList, &m_blackboard);

    m_render.recordingCommandList = nullptr;
    m_render.resources = nullptr;
}

}  // namespace rendering
}  // namespace eyelib
