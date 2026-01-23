#pragma once

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

    bool Initialize();
    void Shutdown();

    void Clear(float r, float g, float b, float a);

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

