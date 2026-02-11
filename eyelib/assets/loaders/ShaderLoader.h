#pragma once

#include "eyelib/assets/AssetId.h"

#include <string>

namespace eyelib {
namespace assets {

class ShaderLoader {
public:
    /** Load by logical shader name (e.g. passes/forward/lit, passes/deferred/lighting). Backend resolves to opengl/ or diligent/ folder. */
    static AssetId LoadShaderByName(const std::string& shaderName);
    static AssetId LoadShader(const std::string& vertexPath, const std::string& fragmentPath);
    static AssetId LoadComputeShader(const std::string& computePath);
};

} // namespace assets
} // namespace eyelib
