#pragma once

#include <string>

namespace eyelib {
namespace render {

/**
 * Single place that resolves any shader name or path to the logical name
 * (e.g. passes/forward/forward). Used by RHI CreateGraphicsPipelineFromName and
 * Shader::LoadFromFile so every shader goes through one resolver.
 *
 * Accepts:
 * - Full path: assets/shaders/opengl/passes/forward/forward.vert
 * - Path-like: ../shaders/passes/forward/forward, shaders/passes/forward/forward
 * - Already logical: passes/forward/forward
 */
inline std::string ShaderPathToLogicalName(const std::string& nameOrPath) {
    if (nameOrPath.empty()) return std::string();
    std::string name = nameOrPath;
    auto stripExt = [](std::string& s, const char* ext, size_t len) {
        if (s.size() >= len && s.compare(s.size() - len, len, ext) == 0) s.resize(s.size() - len);
    };
    stripExt(name, ".vert", 5);
    stripExt(name, ".frag", 5);
    stripExt(name, ".vsh", 4);
    stripExt(name, ".psh", 4);
    const std::string shadersPrefix("shaders/");
    const size_t pos = name.rfind(shadersPrefix);
    if (pos != std::string::npos)
        name = name.substr(pos + shadersPrefix.size());
    if (name.size() > 7 && name.compare(0, 7, "opengl/") == 0)
        name = name.substr(7);
    else if (name.size() > 8 && name.compare(0, 8, "diligent/") == 0)
        name = name.substr(8);
    return name;
}

/** Overload for const char* (e.g. from CreateGraphicsPipelineFromName). */
inline std::string ShaderPathToLogicalName(const char* nameOrPath) {
    if (!nameOrPath || !nameOrPath[0]) return std::string();
    return ShaderPathToLogicalName(std::string(nameOrPath));
}

} // namespace render
} // namespace eyelib
