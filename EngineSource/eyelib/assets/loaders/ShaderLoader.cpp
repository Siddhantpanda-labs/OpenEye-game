#include "ShaderLoader.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/render/ShaderDescriptor.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Shader.h"

namespace eyelib {
namespace assets {

AssetId ShaderLoader::LoadShaderByNameDescriptorOnly(const std::string& shaderName) {
    if (shaderName.empty()) return AssetId::Invalid();
    auto& manager = AssetManager::Get();
    const std::string key = "name:" + shaderName;
    const AssetId id = manager.Registry().RegisterAsset(key, "Shader");
    render::ShaderDescriptor desc;
    desc.shaderName = shaderName;
    desc.loadByName = true;
    desc.layout = render::Mesh::GetVertexLayoutRHI();
    desc.topology = render::rhi::PrimitiveTopology::TriangleList;
    manager.StoreShaderDescriptor(id, desc);
    return id;
}

AssetId ShaderLoader::LoadShaderDescriptorOnly(const std::string& vertexPath, const std::string& fragmentPath) {
    auto& manager = AssetManager::Get();
    const std::string key = vertexPath + "|" + fragmentPath;
    const AssetId id = manager.Registry().RegisterAsset(key, "Shader");
    render::ShaderDescriptor desc;
    desc.vertexPath = vertexPath;
    desc.fragmentPath = fragmentPath;
    desc.loadByName = false;
    desc.layout = render::Mesh::GetVertexLayoutRHI();
    desc.topology = render::rhi::PrimitiveTopology::TriangleList;
    manager.StoreShaderDescriptor(id, desc);
    return id;
}

AssetId ShaderLoader::LoadShaderByName(const std::string& shaderName) {
    if (shaderName.empty()) return AssetId::Invalid();
    auto& manager = AssetManager::Get();
    render::rhi::IDevice* device = static_cast<render::rhi::IDevice*>(manager.GetRenderDevice());
    if (!device) return AssetId::Invalid();
    const std::string key = "name:" + shaderName;
    const AssetId id = manager.Registry().RegisterAsset(key, "Shader");
    auto shader = std::make_shared<render::Shader>();
    if (shader->Load(device, shaderName, render::Mesh::GetVertexLayoutRHI())) {
        manager.Cache().Store(id, shader, "Shader");
        manager.StoreShaderDescriptor(id, shader->GetDescriptor());
        manager.RegisterShaderInstance(id, device, shader);
    }
    return id;
}

AssetId ShaderLoader::LoadShader(const std::string& vertexPath, const std::string& fragmentPath) {
    auto& manager = AssetManager::Get();
    render::rhi::IDevice* device = static_cast<render::rhi::IDevice*>(manager.GetRenderDevice());
    const std::string key = vertexPath + "|" + fragmentPath;
    const AssetId id = manager.Registry().RegisterAsset(key, "Shader");
    auto shader = std::make_shared<render::Shader>();
    bool ok = device && shader->LoadFromFile(device, vertexPath, fragmentPath, render::Mesh::GetVertexLayoutRHI());
    if (ok) {
        manager.Cache().Store(id, shader, "Shader");
        manager.StoreShaderDescriptor(id, shader->GetDescriptor());
        if (device) manager.RegisterShaderInstance(id, device, shader);
    }
    return id;
}

AssetId ShaderLoader::LoadComputeShader(const std::string& computePath) {
    (void)computePath;
    return AssetId::Invalid();
}

} // namespace assets
} // namespace eyelib
