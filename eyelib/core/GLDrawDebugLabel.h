#pragma once

namespace eyelib {
namespace core {

/** Set the current OpenGL draw pass label (for debug callback when GL error 1282). */
void SetGLDrawDebugLabel(const char* label);

/** Get the current label; returns nullptr if none set. */
const char* GetGLDrawDebugLabel();

} // namespace core
} // namespace eyelib
