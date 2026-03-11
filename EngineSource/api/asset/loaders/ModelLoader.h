#pragma once

#include "api/asset/ModelAsset.h"

#include <memory>
#include <string>

namespace eyelib { namespace api { class AssetRegistry; } }

namespace eyelib {
namespace api {

class ModelLoader {
public:
    static std::shared_ptr<ModelAsset> LoadModel(AssetRegistry* registry, const std::string& path);
    static std::shared_ptr<ModelAsset> LoadModel(const std::string& path);
};

} // namespace api
} // namespace eyelib
