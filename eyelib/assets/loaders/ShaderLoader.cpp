#include "ShaderLoader.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/render/resources/Shader.h"

namespace eyelib {
namespace assets {

AssetId ShaderLoader::LoadShader(const std::string& vertexPath, const std::string& fragmentPath) {
    auto& manager = AssetManager::Get();
    const std::string key = vertexPath + "|" + fragmentPath;
    const AssetId id = manager.Registry().RegisterAsset(key, "Shader");
    auto shader = std::make_shared<render::Shader>();
    if (shader->LoadFromFile(vertexPath, fragmentPath)) {
        manager.Cache().Store(id, shader, "Shader");
    }
    return id;
}

AssetId ShaderLoader::LoadComputeShader(const std::string& computePath) {
    (void)computePath;
    return AssetId::Invalid();
}

} // namespace assets
} // namespace eyelib
