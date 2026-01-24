#pragma once

#include "eyelib/assets/AssetId.h"

#include <string>

namespace eyelib {
namespace assets {

class ShaderLoader {
public:
    static AssetId LoadShader(const std::string& vertexPath, const std::string& fragmentPath);
    static AssetId LoadComputeShader(const std::string& computePath);
};

} // namespace assets
} // namespace eyelib
