#pragma once

#include "api/render/RenderDevice.h"

namespace eyelib {
namespace api {

/**
 * Opaque render context (device + command list) for API use (Phase 4).
 * Implementation may hold rhi::IDevice* and rhi::ICommandList*.
 * Pass from pipeline to API-level render calls when context-based rendering is used.
 */
struct RenderContext {
    RenderContext() = default;
    explicit RenderContext(RenderDevice device, void* commandList = nullptr)
        : m_device(device), m_commandList(commandList) {}

    RenderDevice GetDevice() const { return m_device; }
    void* GetCommandList() const { return m_commandList; }
    explicit operator bool() const { return m_device && m_commandList; }

private:
    RenderDevice m_device;
    void* m_commandList = nullptr;
};

} // namespace api
} // namespace eyelib
