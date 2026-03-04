#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {

/**
 * Bundles device and command list for a frame or pass (Phase 4).
 * Pass into render passes instead of ad-hoc device + GetImmediateCommandList().
 * Nothing fetches global state; context is explicit.
 */
struct RenderContext {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;

    /** Build context from device (cmd = device->GetImmediateCommandList()). */
    static RenderContext FromDevice(rhi::IDevice* d) {
        RenderContext ctx;
        ctx.device = d;
        ctx.cmd = d ? d->GetImmediateCommandList() : nullptr;
        return ctx;
    }

    explicit operator bool() const { return device != nullptr && cmd != nullptr; }
};

} // namespace render
} // namespace eyelib
