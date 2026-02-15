#pragma once

namespace eyelib {
namespace render {

/** Runtime render backend selection. Used by CreateGraphicsDevice and pipeline init. */
enum class RenderBackend {
    /** Use default (OpenGL). */
    Default,
    OpenGL,
    Diligent
};


} // namespace render
} // namespace eyelib
