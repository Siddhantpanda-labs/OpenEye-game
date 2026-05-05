#pragma once

#include "eyelib/rendering/shaders/ShaderKey.h"
#include <string>
#include <vector>

namespace eyelib {
namespace rendering {

/** Parameter type for material shader layout (editor-driven UI). */
enum class ShaderParameterType : uint8_t {
    Float,
    Vec3,
    Vec4,
    Texture2D,
};

/** Single parameter in a MaterialShader layout. */
struct ShaderParameter {
    std::string name;
    ShaderParameterType type = ShaderParameterType::Float;
    int textureSlot = -1;
    int uniformSlot = -1;
};

/**
 * MaterialShader: single source of truth for a material type (PBR, Unlit, etc.).
 * Pipeline variants (GBuffer, Forward, ForwardPlus, Shadow) are generated from this.
 * Material holds MaterialShaderId + parameter values; no owned Shader.
 */
struct MaterialShader {
    MaterialTypeId id = kDefaultMaterialTypeId;
    std::string name;
    std::vector<ShaderParameter> parameters;

    bool IsValid() const { return !name.empty(); }
};

/** Registry of built-in material shaders. Thread-safe read. */
class MaterialShaderRegistry {
public:
    static const MaterialShader* Get(MaterialTypeId id);
    static MaterialTypeId Register(MaterialShader shader);
    static void Clear();
private:
    static std::vector<MaterialShader> s_shaders;
};

/** Built-in material shader ids. */
namespace MaterialShaderIds {
    constexpr MaterialTypeId PBR = 0;
    constexpr MaterialTypeId Unlit = 1;
}

} // namespace rendering
} // namespace eyelib
