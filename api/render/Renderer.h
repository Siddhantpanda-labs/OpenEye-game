#pragma once

#include "api/render/RenderDevice.h"

namespace eyelib {
namespace api {

/**
 * @brief Client-facing renderer wrapper (no OpenGL types).
 *
 * Requires an active platform window/OpenGL context.
 */
class Renderer {
public:
    Renderer();
    ~Renderer();

    /** Set device before Clear (for default framebuffer clear). */
    void SetDevice(RenderDevice device);
    bool Initialize();
    void Shutdown();

    void Clear(float r, float g, float b, float a);

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

