#pragma once

namespace eyelib {
namespace render {

/** Runtime render backend selection. Used by CreateGraphicsDevice and pipeline init. */
enum class RenderBackend {
    /** Use default (OpenGL when no window / BGFX, Diligent or OpenGL based on build and preference). */
    Default,
    OpenGL,
    BGFX,
    Diligent
};


} // namespace render
} // namespace eyelib
