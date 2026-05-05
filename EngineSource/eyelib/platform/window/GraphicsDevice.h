#pragma once

#include "eyelib/render/RenderBackend.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace platform {

class Window;

/**
 * Single entry point to create an RHI device. Backend is chosen at runtime.
 * If window is null (e.g. headless), returns nullptr.
 * Default backend is OpenGL. Diligent is only used when explicitly requested and built (e.g. editor); player builds use OpenGL only.
 */
std::unique_ptr<render::rhi::IDevice> CreateGraphicsDevice(
    Window* window,
    uint32_t width,
    uint32_t height,
    render::RenderBackend backend = render::RenderBackend::Default);

} // namespace platform
} // namespace eyelib
