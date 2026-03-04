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

    /** CPU-only: store ShaderDescriptor by name; no compile or GPU. For binding phase. */
    static AssetId LoadShaderByNameDescriptorOnly(const std::string& shaderName);
    /** CPU-only: store ShaderDescriptor by paths; no compile or GPU. For binding phase. */
    static AssetId LoadShaderDescriptorOnly(const std::string& vertexPath, const std::string& fragmentPath);
};

} // namespace assets
} // namespace eyelib
