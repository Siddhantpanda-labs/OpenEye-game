#include "api/render/ShaderLoader.h"
#include "api/asset/AssetRegistry.h"

namespace eyelib {
namespace api {

AssetId ShaderLoader::LoadShader(AssetRegistry* registry, const std::string& vertexPath, const std::string& fragmentPath) {
    if (!registry) return AssetId::Invalid();
    const std::string key = vertexPath + "|" + fragmentPath;
    registry->LoadAsset(key, AssetType::Shader);
    return registry->GetId(key);
}

AssetId ShaderLoader::LoadShader(const std::string& vertexPath, const std::string& fragmentPath) {
    (void)vertexPath;
    (void)fragmentPath;
    return AssetId::Invalid();
}

AssetId ShaderLoader::LoadComputeShader(const std::string& computePath) {
    (void)computePath;
    return AssetId::Invalid();
}

} // namespace api
} // namespace eyelib
