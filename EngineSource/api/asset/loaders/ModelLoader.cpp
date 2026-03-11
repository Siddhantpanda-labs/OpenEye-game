#include "ModelLoader.h"

namespace eyelib {
namespace api {

std::shared_ptr<ModelAsset> ModelLoader::LoadModel(AssetRegistry* registry, const std::string& path) {
    (void)registry;
    (void)path;
    return nullptr;
}

std::shared_ptr<ModelAsset> ModelLoader::LoadModel(const std::string& path) {
    (void)path;
    return nullptr;
}

} // namespace api
} // namespace eyelib
