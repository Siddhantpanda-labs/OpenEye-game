#pragma once

#include "api/assets/ModelAsset.h"

#include <memory>
#include <string>

namespace eyelib {
namespace api {

class ModelLoader {
public:
    static std::shared_ptr<ModelAsset> LoadModel(const std::string& path);
};

} // namespace api
} // namespace eyelib
