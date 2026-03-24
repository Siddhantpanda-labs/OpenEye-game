#include "eyelib/render/ShaderLoader.h"
#include "eyelib/render/resources/Mesh.h"

namespace eyelib {
namespace render {

ShaderDescriptor ShaderLoader::BuildDescriptorFromAsset(const std::string& assetPath) {
    ShaderDescriptor desc;
    desc.assetPath = assetPath;
    desc.loadFromAsset = true;
    desc.layout = Mesh::GetVertexLayoutRHI();
    desc.topology = rhi::PrimitiveTopology::TriangleList;
    return desc;
}

ShaderDescriptor ShaderLoader::BuildDescriptorFromPaths(const std::string& vertexPath, const std::string& fragmentPath) {
    ShaderDescriptor desc;
    desc.vertexPath = vertexPath;
    desc.fragmentPath = fragmentPath;
    desc.loadFromAsset = false;
    desc.layout = Mesh::GetVertexLayoutRHI();
    desc.topology = rhi::PrimitiveTopology::TriangleList;
    return desc;
}

} // namespace render
} // namespace eyelib
