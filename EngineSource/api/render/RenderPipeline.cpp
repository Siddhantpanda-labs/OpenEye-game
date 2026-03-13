#include "api/render/RenderPipeline.h"
#include "api/render/RenderSceneAdapter.h"
#include "api/render/RenderTarget.h"
#include "eyelib/render/RenderTarget.h"
#include "api/platform/Window.h"
#include "api/core/All.h"
#include "eyelib/render/2d/Primitives.h"
#include "eyelib/render/DebugDrawRenderer.h"
#include "eyelib/render/renderer/RenderPipeline.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/platform/window/GraphicsDevice.h"

namespace eyelib {
namespace api {

class RenderPipeline::Impl {
public:
    std::unique_ptr<::eyelib::render::RenderPipeline> pipeline = std::make_unique<::eyelib::render::RenderPipeline>();
};

RenderPipeline::RenderPipeline() : m_impl(std::make_unique<Impl>()) {}

RenderPipeline::~RenderPipeline() = default;

RenderPipeline::RenderPipeline(RenderPipeline&& other) noexcept = default;

RenderPipeline& RenderPipeline::operator=(RenderPipeline&& other) noexcept = default;

bool RenderPipeline::Initialize(Window& window, uint32_t width, uint32_t height,
                                ::eyelib::render::RenderBackend backend) {
    const char* backendName = (backend == ::eyelib::render::RenderBackend::Diligent) ? "Diligent" :
                              (backend == ::eyelib::render::RenderBackend::OpenGL) ? "OpenGL" : "Default";
    core::Log::InfoF("RenderPipeline::Initialize: backend=%s width=%u height=%u", backendName, width, height);

    void* platformWindow = window.GetPlatformWindow();
    if (!platformWindow) {
        core::Log::Error("RenderPipeline::Initialize: platform window is null");
        return false;
    }

    auto device = ::eyelib::platform::CreateGraphicsDevice(
        static_cast<::eyelib::platform::Window*>(platformWindow), width, height, backend);
    if (!device) {
        core::Log::Error("RenderPipeline::Initialize: CreateGraphicsDevice returned null");
        return false;
    }

    if (!m_impl->pipeline->Initialize(std::move(device), width, height)) {
        core::Log::Error("RenderPipeline::Initialize: pipeline->Initialize(device, width, height) failed");
        return false;
    }
    core::Log::Info("RenderPipeline::Initialize: success");
    return true;
}

bool RenderPipeline::InitializeWithSharedDevice(RenderDevice device, uint32_t width, uint32_t height) {
    if (!device) {
        core::Log::Error("RenderPipeline::InitializeWithSharedDevice: device is null");
        return false;
    }
    auto* rhiDevice = static_cast<::eyelib::render::rhi::IDevice*>(device.Get());
    if (!m_impl->pipeline->Initialize(rhiDevice, width, height)) {
        core::Log::Error("RenderPipeline::InitializeWithSharedDevice: pipeline init failed");
        return false;
    }
    core::Log::Info("RenderPipeline::InitializeWithSharedDevice: success");
    return true;
}

void RenderPipeline::Resize(uint32_t width, uint32_t height) {
    m_impl->pipeline->Resize(width, height);
}

void RenderPipeline::Resize(const RenderTarget& target) {
    Resize(target.GetWidth(), target.GetHeight());
}

RenderDevice RenderPipeline::GetDevice() const {
    return m_impl->pipeline ? RenderDevice(m_impl->pipeline->GetRHIDevice()) : RenderDevice();
}

void* RenderPipeline::GetOutputColorTextureNativeHandle() const {
    if (!m_impl->pipeline) return nullptr;
    ::eyelib::render::rhi::ITexture* tex = m_impl->pipeline->GetCurrentFrameOutputColorTextureRHI();
    return tex ? tex->GetNativeTexture() : nullptr;
}

void RenderPipeline::RestoreDefaultFramebufferAndViewport(uint32_t width, uint32_t height) {
    if (!m_impl->pipeline) return;
    ::eyelib::render::rhi::ICommandList* cmd = m_impl->pipeline->GetRHICommandList();
    if (!cmd) return;
    cmd->SetRenderTarget(nullptr);
    cmd->SetViewport(0, 0, width, height);
}

std::vector<RenderPipeline::FrameGraphPassInfo> RenderPipeline::GetFrameGraphPasses() const {
    std::vector<FrameGraphPassInfo> out;
    if (!m_impl->pipeline) return out;
    const auto& passes = m_impl->pipeline->GetFrameGraphPasses();
    out.reserve(passes.size());
    for (const auto& p : passes) {
        FrameGraphPassInfo info;
        info.name = p.name;
        info.gpuMs = p.gpuMs;
        info.culled = p.culled;
        out.push_back(info);
    }
    return out;
}

void* RenderPipeline::InsertFrameFence() {
    if (!m_impl->pipeline) return nullptr;
    auto* cmd = m_impl->pipeline->GetRHICommandList();
    if (!cmd) return nullptr;
    return cmd->InsertFence();
}

void RenderPipeline::WaitFence(void* fence) {
    if (!fence || !m_impl->pipeline) return;
    auto* device = m_impl->pipeline->GetRHIDevice();
    if (!device) return;
    auto* f = static_cast<::eyelib::render::rhi::IFence*>(fence);
    device->WaitFence(f, 5000);
}

void RenderPipeline::WaitAndDestroyFence(void* fence) {
    if (!fence || !m_impl->pipeline) return;
    auto* device = m_impl->pipeline->GetRHIDevice();
    if (!device) return;
    auto* f = static_cast<::eyelib::render::rhi::IFence*>(fence);
    device->WaitFence(f, 5000);
    device->DestroyFence(f);
}

void RenderPipeline::Shutdown() {
    if (m_impl->pipeline->OwnsDevice()) {
        ::eyelib::render::DebugDrawRenderer::Shutdown();
        ::eyelib::render::Primitives::ShutdownStaticResources();
    }
    m_impl->pipeline->Shutdown();
}

bool RenderPipeline::OwnsDevice() const {
    return m_impl->pipeline && m_impl->pipeline->OwnsDevice();
}

void RenderPipeline::SetMode(Mode mode) {
    ::eyelib::render::PipelineMode m = (mode == Mode::Forward) ? ::eyelib::render::PipelineMode::Forward :
                             (mode == Mode::Deferred) ? ::eyelib::render::PipelineMode::Deferred :
                             ::eyelib::render::PipelineMode::ForwardPlus;
    m_impl->pipeline->SetMode(m);
}

RenderPipeline::Mode RenderPipeline::GetMode() const {
    ::eyelib::render::PipelineMode m = m_impl->pipeline->GetMode();
    return (m == ::eyelib::render::PipelineMode::Forward) ? Mode::Forward :
           (m == ::eyelib::render::PipelineMode::Deferred) ? Mode::Deferred :
           Mode::ForwardPlus;
}

void RenderPipeline::Render(const RenderScene& scene, const std::vector<uint32_t>* entityIdsForOpaque) {
    ApiRenderSceneView view(scene);
    m_impl->pipeline->Render(view, entityIdsForOpaque);
}

void RenderPipeline::RenderStereo(const RenderScene& scene, const CameraView& leftCam, const CameraView& rightCam) {
    ApiRenderSceneView view(scene);
    ::eyelib::render::CameraView left, right;
    left.view = leftCam.view;
    left.projection = leftCam.projection;
    left.viewPos = leftCam.viewPos;
    left.nearPlane = leftCam.nearPlane;
    left.farPlane = leftCam.farPlane;
    right.view = rightCam.view;
    right.projection = rightCam.projection;
    right.viewPos = rightCam.viewPos;
    right.nearPlane = rightCam.nearPlane;
    right.farPlane = rightCam.farPlane;
    m_impl->pipeline->RenderStereo(view, left, right);
}

bool RenderPipeline::ReadPixelEntityId(uint32_t x, uint32_t y, uint32_t* outId) const {
    if (!outId) return false;
    *outId = 0;
    ::eyelib::render::rhi::IRenderTarget* rt = m_impl->pipeline->GetEntityIdRenderTarget();
    ::eyelib::render::rhi::IDevice* device = m_impl->pipeline->GetRHIDevice();
    if (!rt || !device) return false;
    return device->ReadPixelUint32(rt, x, y, outId);
}

bool RenderPipeline::ReadOutputPixels(uint32_t width, uint32_t height, uint8_t* outRGBA) const {
    return m_impl->pipeline && m_impl->pipeline->ReadOutputPixels(width, height, outRGBA);
}

bool RenderPipeline::ReadPixelsFromRenderTarget(RenderTarget& target, uint8_t* outRGBA) const {
    if (!m_impl->pipeline) return false;
    auto* device = m_impl->pipeline->GetRHIDevice();
    auto internal = target.GetInternal();
    if (!device || !internal || !outRGBA) return false;
    ::eyelib::render::rhi::IRenderTarget* rt = internal->GetRenderTargetRHI();
    if (!rt) return false;
    const uint32_t w = target.GetWidth();
    const uint32_t h = target.GetHeight();
    return device->ReadPixelsRGBA8(rt, 0, 0, w, h, outRGBA);
}

void* RenderPipeline::CreatePixelPackBuffer(size_t sizeInBytes) const {
    if (!m_impl->pipeline) return nullptr;
    auto* device = m_impl->pipeline->GetRHIDevice();
    return device ? device->CreatePixelPackBuffer(sizeInBytes) : nullptr;
}

void RenderPipeline::DestroyPixelPackBuffer(void* handle) const {
    if (!m_impl->pipeline || !handle) return;
    auto* device = m_impl->pipeline->GetRHIDevice();
    if (device) device->DestroyPixelPackBuffer(handle);
}

void RenderPipeline::ReadPixelsToPBO(RenderTarget& target, void* pboHandle) const {
    if (!m_impl->pipeline || !pboHandle) return;
    auto* device = m_impl->pipeline->GetRHIDevice();
    auto internal = target.GetInternal();
    if (!device || !internal) return;
    ::eyelib::render::rhi::IRenderTarget* rt = internal->GetRenderTargetRHI();
    if (!rt) return;
    const uint32_t w = target.GetWidth();
    const uint32_t h = target.GetHeight();
    device->ReadPixelsToPBO(rt, 0, 0, w, h, pboHandle);
}

void RenderPipeline::MapPixelPackBuffer(void* pboHandle, void* outBuffer, size_t sizeInBytes, uint32_t width, uint32_t height) const {
    if (!m_impl->pipeline) return;
    auto* device = m_impl->pipeline->GetRHIDevice();
    if (device) device->MapPixelPackBuffer(pboHandle, outBuffer, sizeInBytes, width, height);
}

void RenderPipeline::MapPixelPackBufferRaw(void* pboHandle, void* outBuffer, size_t sizeInBytes) const {
    if (!m_impl->pipeline) return;
    auto* device = m_impl->pipeline->GetRHIDevice();
    if (device) device->MapPixelPackBufferRaw(pboHandle, outBuffer, sizeInBytes);
}

void RenderPipeline::RunStreamingNV12Pass(RenderTarget& sourceRT, void* nv12PBOHandle, size_t nv12Size, uint32_t width, uint32_t height, uint32_t pitch) const {
    if (!m_impl->pipeline || !nv12PBOHandle || nv12Size == 0 || pitch == 0) return;
    auto internal = sourceRT.GetInternal();
    if (!internal) return;
    ::eyelib::render::rhi::IRenderTarget* rt = internal->GetRenderTargetRHI();
    if (!rt) return;
    m_impl->pipeline->RunStreamingNV12Pass(rt, nv12PBOHandle, nv12Size, width, height, pitch);
}

void RenderPipeline::RenderOutlineFromIdBuffer(uint32_t selectedPackedId, float r, float g, float b) {
    if (selectedPackedId == 0) return;
    m_impl->pipeline->RenderOutlineFromIdBuffer(selectedPackedId, r, g, b);
}

void RenderPipeline::RenderDebugLines(const eyelib::math::Mat4& view, const eyelib::math::Mat4& proj,
    const std::vector<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>>& lines, float r, float g, float b) {
    m_impl->pipeline->RenderDebugLines(view, proj, lines, r, g, b);
}

void RenderPipeline::SetPendingDebugLines(const eyelib::math::Mat4& view, const eyelib::math::Mat4& proj,
    const std::vector<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>>& lines, float r, float g, float b) {
    m_impl->pipeline->SetPendingDebugLines(view, proj, lines, r, g, b);
}

void RenderPipeline::SetPendingDebugLinesColored(const eyelib::math::Mat4& view, const eyelib::math::Mat4& proj,
    const std::vector<std::pair<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>, eyelib::math::Vec4>>& linesWithColors) {
    m_impl->pipeline->SetPendingDebugLinesColored(view, proj, linesWithColors);
}

void RenderPipeline::ReloadShaders() {
    m_impl->pipeline->ReloadShaders();
}

void RenderPipeline::BlitToScreen() {
    m_impl->pipeline->BlitToScreen();
}

void RenderPipeline::BlitOutputTo(RenderTarget& target) {
    if (!m_impl->pipeline) return;
    auto* device = m_impl->pipeline->GetRHIDevice();
    auto* srcRT = m_impl->pipeline->GetCurrentFrameOutputRT();
    auto* destRT = target.GetInternal() ? target.GetInternal()->GetRenderTargetRHI() : nullptr;
    if (device && srcRT && destRT)
        device->Blit(srcRT, destRT, true);
}

void RenderPipeline::BlitStereoToScreen(RenderTarget& left, RenderTarget& right, uint32_t totalWidth, uint32_t totalHeight) {
    if (!m_impl->pipeline || totalWidth < 2 || totalHeight < 1) return;
    auto* device = m_impl->pipeline->GetRHIDevice();
    auto* rtLeft = left.GetInternal() ? left.GetInternal()->GetRenderTargetRHI() : nullptr;
    auto* rtRight = right.GetInternal() ? right.GetInternal()->GetRenderTargetRHI() : nullptr;
    if (!device || !rtLeft || !rtRight) return;
    const int halfW = static_cast<int>(totalWidth / 2);
    device->BlitToDefaultFramebufferSubregion(rtLeft, 0, 0, halfW, static_cast<int>(totalHeight));
    device->BlitToDefaultFramebufferSubregion(rtRight, halfW, 0, halfW, static_cast<int>(totalHeight));
}

void RenderPipeline::Finish() {
    m_impl->pipeline->Finish();
}

bool RenderPipeline::HasValidOutput() const {
    return m_impl->pipeline->HasValidOutput();
}

} // namespace api
} // namespace eyelib
