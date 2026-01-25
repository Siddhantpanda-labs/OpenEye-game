#pragma once

#include "api/render/RenderScene.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {

class RenderTarget;

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
    bool Initialize(const RenderTarget& target);
    void Resize(uint32_t width, uint32_t height);
    void Resize(const RenderTarget& target);
    void Shutdown();

    void SetMode(Mode mode);
    Mode GetMode() const;

    /** Render scene. */
    void Render(const RenderScene& scene);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
