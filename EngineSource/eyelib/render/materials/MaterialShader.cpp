#include "eyelib/render/materials/MaterialShader.h"

namespace eyelib {
namespace render {

std::vector<MaterialShader> MaterialShaderRegistry::s_shaders;

static bool s_builtinRegistered = false;

static void RegisterBuiltins() {
    if (s_builtinRegistered) return;
    s_builtinRegistered = true;
    MaterialShader pbr;
    pbr.id = MaterialShaderIds::PBR;
    pbr.name = "PBR";
    pbr.parameters = {
        {"u_Albedo", ShaderParameterType::Vec3, -1, -1},
        {"u_Metallic", ShaderParameterType::Float, -1, -1},
        {"u_Roughness", ShaderParameterType::Float, -1, -1},
        {"u_AO", ShaderParameterType::Float, -1, -1},
        {"u_Emission", ShaderParameterType::Vec3, -1, -1},
        {"u_Alpha", ShaderParameterType::Float, -1, -1},
        {"u_AlbedoTexture", ShaderParameterType::Texture2D, 0, -1},
        {"u_NormalTexture", ShaderParameterType::Texture2D, 1, -1},
        {"u_MetallicRoughnessTexture", ShaderParameterType::Texture2D, 2, -1},
        {"u_AOTexture", ShaderParameterType::Texture2D, 3, -1},
        {"u_EmissiveTexture", ShaderParameterType::Texture2D, 4, -1},
    };
    MaterialShaderRegistry::Register(pbr);
    MaterialShader unlit;
    unlit.id = MaterialShaderIds::Unlit;
    unlit.name = "Unlit";
    unlit.parameters = {
        {"u_Albedo", ShaderParameterType::Vec3, -1, -1},
        {"u_Alpha", ShaderParameterType::Float, -1, -1},
        {"u_AlbedoTexture", ShaderParameterType::Texture2D, 0, -1},
    };
    MaterialShaderRegistry::Register(unlit);
}

const MaterialShader* MaterialShaderRegistry::Get(MaterialTypeId id) {
    RegisterBuiltins();
    for (const auto& s : s_shaders)
        if (s.id == id) return &s;
    return nullptr;
}

MaterialTypeId MaterialShaderRegistry::Register(MaterialShader shader) {
    RegisterBuiltins();
    for (const auto& s : s_shaders)
        if (s.id == shader.id) return shader.id;
    if (shader.id == kDefaultMaterialTypeId && !s_shaders.empty()) {
        MaterialTypeId next = 0;
        for (const auto& s : s_shaders)
            if (s.id >= next) next = s.id + 1;
        shader.id = next;
    }
    s_shaders.push_back(std::move(shader));
    return s_shaders.back().id;
}

void MaterialShaderRegistry::Clear() {
    s_shaders.clear();
    s_builtinRegistered = false;
}

} // namespace render
} // namespace eyelib
