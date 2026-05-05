#pragma once

#include "api/asset/AssetId.h"

#include <functional>
#include <string>

namespace eyelib { namespace api { class AssetRegistry; } }

namespace eyelib {
namespace api {

class MeshLoader {
public:
    using LoadCallback = std::function<void(AssetId)>;

    static AssetId LoadMesh(AssetRegistry* registry, const std::string& path);
    static AssetId LoadMesh(const std::string& path);
    static void LoadMeshAsync(const std::string& path, LoadCallback callback);
};

} // namespace api
} // namespace eyelib
