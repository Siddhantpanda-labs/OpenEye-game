#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace rendering {

/** Reflected uniform/sampler from shader source. E5: Shader system reflection. */
enum class ShaderUniformType {
    Int,
    Float,
    Vec2,
    Vec3,
    Vec4,
    Mat4,
    Mat4Array,
    Sampler2D,
    SamplerCube,
    Sampler2DShadow,
    Unknown,
};

struct ShaderUniformInfo {
    std::string name;
    ShaderUniformType type = ShaderUniformType::Unknown;
    /** For arrays (e.g. mat4[128]), the element count; 0 for non-arrays. */
    int arrayCount = 0;
    /** Optional: UBO block name if this is part of a block (e.g. "Bones"). */
    std::string blockName;
};

/** Result of reflecting vertex + fragment (or other stage) source. */
struct ShaderReflection {
    std::vector<ShaderUniformInfo> uniforms;

    const ShaderUniformInfo* Find(const std::string& name) const;
    bool HasUniform(const std::string& name) const;
};

/**
 * Parse combined GLSL source for "uniform type name;" and "uniform type name[N];"
 * and optional "layout(...) uniform BlockName { ... };". Fills uniforms (standalone + block members).
 * Used for E5.1 reflection; does not run a full preprocessor.
 */
ShaderReflection ParseShaderReflection(const std::string& vertexSrc, const std::string& fragmentSrc);

/** Parse a single GLSL source string for uniforms (e.g. compute or single-stage). */
ShaderReflection ParseShaderReflectionFromSource(const std::string& source);

} // namespace rendering
} // namespace eyelib
