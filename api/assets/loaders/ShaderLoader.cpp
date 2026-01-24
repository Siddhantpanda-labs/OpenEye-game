#include "ShaderLoader.h"

#include "eyelib/assets/loaders/ShaderLoader.h"

namespace eyelib {
namespace api {

namespace {
AssetId ToApi(const assets::AssetId& id) {
    AssetId out;
    out.high = id.High();
    out.low = id.Low();
    return out;
}
} // namespace

AssetId ShaderLoader::LoadShader(const std::string& vertexPath, const std::string& fragmentPath) {
    return ToApi(assets::ShaderLoader::LoadShader(vertexPath, fragmentPath));
}

AssetId ShaderLoader::LoadComputeShader(const std::string& computePath) {
    return ToApi(assets::ShaderLoader::LoadComputeShader(computePath));
}

} // namespace api
} // namespace eyelib
