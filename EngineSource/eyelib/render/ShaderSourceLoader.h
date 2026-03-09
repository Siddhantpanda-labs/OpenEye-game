#pragma once

#include <string>

namespace eyelib {
namespace render {

/** Load shader source from path with #include expansion. Used by Shader and OpenGL backend. */
bool LoadShaderSource(const std::string& path, std::string& out);

} // namespace render
} // namespace eyelib
