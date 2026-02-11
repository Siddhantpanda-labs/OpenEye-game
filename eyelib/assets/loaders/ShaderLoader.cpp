#include "ShaderLoader.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/rhi/RHIContext.h"

namespace eyelib {
namespace assets {

AssetId ShaderLoader::LoadShaderByName(const std::string& shaderName) {
    if (shaderName.empty()) return AssetId::Invalid();
    auto& manager = AssetManager::Get();
    const std::string key = "name:" + shaderName;
    const AssetId id = manager.Registry().RegisterAsset(key, "Shader");
    auto shader = std::make_shared<render::Shader>();
    render::rhi::IDevice* device = render::rhi::GetDefaultDevice();
    if (device && shader->Load(device, shaderName, render::Mesh::GetVertexLayoutRHI())) {
        manager.Cache().Store(id, shader, "Shader");
    }
    return id;
}

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
