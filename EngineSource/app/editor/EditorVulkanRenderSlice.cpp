#include "EditorVulkanRenderSlice.h"

#include "eyelib/render/RenderBackend.h"

namespace openeye::editor {

VulkanRenderSliceScope::VulkanRenderSliceScope(eyelib::render::rhi::IDevice* device) : m_device(device) {
    using eyelib::render::RenderBackend;
    if (!m_device || m_device->GetRenderBackend() != RenderBackend::Vulkan)
        return;
    m_cmd = m_device->CreateCommandList();
    if (!m_cmd)
        return;
    m_cmd->Begin();
    m_device->PushTransientRecordingCommandList(m_cmd);
    m_active = true;
}

VulkanRenderSliceScope::VulkanRenderSliceScope(VulkanRenderSliceScope&& other) noexcept {
    m_device = other.m_device;
    m_cmd = other.m_cmd;
    m_active = other.m_active;
    other.m_device = nullptr;
    other.m_cmd = nullptr;
    other.m_active = false;
}

VulkanRenderSliceScope& VulkanRenderSliceScope::operator=(VulkanRenderSliceScope&& other) noexcept {
    if (this == &other)
        return *this;
    Release();
    m_device = other.m_device;
    m_cmd = other.m_cmd;
    m_active = other.m_active;
    other.m_device = nullptr;
    other.m_cmd = nullptr;
    other.m_active = false;
    return *this;
}

VulkanRenderSliceScope::~VulkanRenderSliceScope() { Release(); }

void VulkanRenderSliceScope::Release() {
    if (!m_active || !m_device || !m_cmd) {
        m_device = nullptr;
        m_cmd = nullptr;
        m_active = false;
        return;
    }
    m_device->PopTransientRecordingCommandList(m_cmd);
    m_cmd->End();
    m_device->EnqueueFrameSliceCommandList(m_cmd);
    m_cmd = nullptr;
    m_active = false;
    m_device = nullptr;
}

} // namespace openeye::editor
