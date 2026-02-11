#pragma once

#include "api/render/RenderScene.h"
#include "eyelib/render/RenderBackend.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {

class Window;
class RenderTarget;

/**
 * @brief Render pipeline API (opaque). Initialize, set mode, render scene.
 * No OpenGL types exposed. Single init path: pass window and backend (e.g. from --renderer).
 */
class RenderPipeline {
public:
    enum class Mode { Forward, Deferred, ForwardPlus };

    RenderPipeline();
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;
    RenderPipeline(RenderPipeline&& other) noexcept;
    RenderPipeline& operator=(RenderPipeline&& other) noexcept;

    /** Create device and pipeline. Requires a window; backend chosen at runtime (e.g. --renderer bgfx|opengl). */
    bool Initialize(Window& window, uint32_t width, uint32_t height,
                    ::eyelib::render::RenderBackend backend = ::eyelib::render::RenderBackend::Default);
    void Resize(uint32_t width, uint32_t height);
    void Resize(const RenderTarget& target);
    void Shutdown();

    void SetMode(Mode mode);
    Mode GetMode() const;

    /** Render scene. */
    void Render(const RenderScene& scene);
    void BlitToScreen();
    bool HasValidOutput() const;

    /** Opaque device handle for creating RHI resources (e.g. RenderTarget). Null if not initialized. */
    void* GetDevice() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
