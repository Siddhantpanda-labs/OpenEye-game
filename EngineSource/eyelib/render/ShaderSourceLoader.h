#pragma once

#include <string>
#include <vector>

namespace eyelib {
namespace render {

/** Load shader source from path with #include expansion. Used by Shader and OpenGL backend. */
bool LoadShaderSource(const std::string& path, std::string& out);

/** Same as LoadShaderSource; if outDependencies is non-null, fills it with all paths read (main + includes, normalized). */
bool LoadShaderSourceWithDependencies(const std::string& path, std::string& out, std::vector<std::string>* outDependencies);

} // namespace render
} // namespace eyelib
