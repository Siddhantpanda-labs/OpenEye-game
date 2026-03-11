#include "eyelib/render/ShaderLoader.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"

namespace eyelib {
namespace render {

ShaderDescriptor ShaderLoader::BuildDescriptorByName(const std::string& shaderName) {
    ShaderDescriptor desc;
    desc.shaderName = shaderName;
    desc.loadByName = true;
    desc.layout = Mesh::GetVertexLayoutRHI();
    desc.topology = rhi::PrimitiveTopology::TriangleList;
    return desc;
}

ShaderDescriptor ShaderLoader::BuildDescriptorFromPaths(const std::string& vertexPath, const std::string& fragmentPath) {
    ShaderDescriptor desc;
    desc.vertexPath = vertexPath;
    desc.fragmentPath = fragmentPath;
    desc.loadByName = false;
    desc.layout = Mesh::GetVertexLayoutRHI();
    desc.topology = rhi::PrimitiveTopology::TriangleList;
    return desc;
}

std::shared_ptr<Shader> ShaderLoader::LoadShader(rhi::IDevice* device, const std::string& shaderName) {
    if (!device || shaderName.empty()) return nullptr;
    auto sh = std::make_shared<Shader>();
    if (!sh->Load(device, shaderName, Mesh::GetVertexLayoutRHI())) return nullptr;
    return sh;
}

std::shared_ptr<Shader> ShaderLoader::LoadShader(rhi::IDevice* device, const std::string& vertexPath, const std::string& fragmentPath) {
    if (!device) return nullptr;
    auto sh = std::make_shared<Shader>();
    if (!sh->LoadFromFile(device, vertexPath, fragmentPath, Mesh::GetVertexLayoutRHI())) return nullptr;
    return sh;
}

} // namespace render
} // namespace eyelib
