#pragma once

#include "api/asset/AssetId.h"

#include <string>

namespace eyelib { namespace api { class AssetRegistry; } }

namespace eyelib {
namespace api {

class ShaderLoader {
public:
    static AssetId LoadShader(AssetRegistry* registry, const std::string& vertexPath, const std::string& fragmentPath);
    static AssetId LoadShader(const std::string& vertexPath, const std::string& fragmentPath);
    static AssetId LoadComputeShader(const std::string& computePath);
};

} // namespace api
} // namespace eyelib
