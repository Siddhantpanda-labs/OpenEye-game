#pragma once

#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {

/**
 * @brief Render pipeline API (opaque). Initialize, set mode, render scene.
 * No OpenGL types exposed.
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

    bool Initialize(uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);
    void Shutdown();

    void SetMode(Mode mode);
    Mode GetMode() const;

    /** Render scene. Scene data passed via RenderScene struct (to be defined). */
    void Render(/* RenderScene scene - to be added */);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
