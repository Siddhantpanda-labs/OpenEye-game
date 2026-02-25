#include "api/render/RenderPipeline.h"
#include "api/render/RenderSceneAdapter.h"
#include "api/render/RenderTarget.h"
#include "api/platform/Window.h"
#include "api/core/All.h"
#include "eyelib/render/RenderPipeline.h"
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

void RenderPipeline::Resize(uint32_t width, uint32_t height) {
    m_impl->pipeline->Resize(width, height);
}

void RenderPipeline::Resize(const RenderTarget& target) {
    Resize(target.GetWidth(), target.GetHeight());
}

void* RenderPipeline::GetDevice() const {
    return m_impl->pipeline ? m_impl->pipeline->GetRHIDevice() : nullptr;
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
    m_impl->pipeline->Shutdown();
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

bool RenderPipeline::ReadPixelEntityId(uint32_t x, uint32_t y, uint32_t* outId) const {
    if (!outId) return false;
    *outId = 0;
    ::eyelib::render::rhi::IRenderTarget* rt = m_impl->pipeline->GetEntityIdRenderTarget();
    ::eyelib::render::rhi::IDevice* device = m_impl->pipeline->GetRHIDevice();
    if (!rt || !device) return false;
    return device->ReadPixelUint32(rt, x, y, outId);
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

void RenderPipeline::Finish() {
    m_impl->pipeline->Finish();
}

bool RenderPipeline::HasValidOutput() const {
    return m_impl->pipeline->HasValidOutput();
}

} // namespace api
} // namespace eyelib
