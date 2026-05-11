#include "api/rendering/RenderSliceScope.h"

// This translation unit is allowed to include `eyelib/rendering/...` per the
// `api/rendering` charter (only place outside `eyelib/rendering/` that may).
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/core/RenderBackend.h"

namespace eyelib {
namespace api {
namespace rendering {

namespace {
inline ::eyelib::rendering::rhi::IDevice* AsRhiDevice(void* opaque) noexcept {
    return static_cast<::eyelib::rendering::rhi::IDevice*>(opaque);
}
inline ::eyelib::rendering::rhi::ICommandList* AsRhiCmd(void* opaque) noexcept {
    return static_cast<::eyelib::rendering::rhi::ICommandList*>(opaque);
}
} // namespace

RenderSliceScope::RenderSliceScope(RenderDevice device) noexcept
    : m_device(device.Get()), m_cmd(nullptr), m_active(false) {
    auto* rhi = AsRhiDevice(m_device);
    if (!rhi) return;
    if (rhi->GetRenderBackend() != ::eyelib::rendering::RenderBackend::Vulkan) {
        m_device = nullptr;
        return;
    }
    auto* cmd = rhi->CreateCommandList();
    if (!cmd) {
        m_device = nullptr;
        return;
    }
    cmd->Begin();
    rhi->PushTransientRecordingCommandList(cmd);
    m_cmd = cmd;
    m_active = true;
}

RenderSliceScope::RenderSliceScope(RenderSliceScope&& other) noexcept
    : m_device(other.m_device), m_cmd(other.m_cmd), m_active(other.m_active) {
    other.m_device = nullptr;
    other.m_cmd = nullptr;
    other.m_active = false;
}

RenderSliceScope& RenderSliceScope::operator=(RenderSliceScope&& other) noexcept {
    if (this == &other) return *this;
    Release();
    m_device = other.m_device;
    m_cmd = other.m_cmd;
    m_active = other.m_active;
    other.m_device = nullptr;
    other.m_cmd = nullptr;
    other.m_active = false;
    return *this;
}

RenderSliceScope::~RenderSliceScope() {
    Release();
}

void RenderSliceScope::Release() noexcept {
    if (!m_active || !m_device || !m_cmd) {
        m_device = nullptr;
        m_cmd = nullptr;
        m_active = false;
        return;
    }
    auto* rhi = AsRhiDevice(m_device);
    auto* cmd = AsRhiCmd(m_cmd);
    rhi->PopTransientRecordingCommandList(cmd);
    cmd->End();
    rhi->EnqueueFrameSliceCommandList(cmd);
    m_cmd = nullptr;
    m_active = false;
    m_device = nullptr;
}

} // namespace rendering
} // namespace api
} // namespace eyelib
