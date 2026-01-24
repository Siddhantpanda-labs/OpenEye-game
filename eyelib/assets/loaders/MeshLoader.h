#pragma once

#include "eyelib/assets/AssetId.h"

#include <functional>
#include <string>

namespace eyelib {
namespace assets {

class MeshLoader {
public:
    using LoadCallback = std::function<void(AssetId)>;

    static AssetId LoadMesh(const std::string& path);
    static void LoadMeshAsync(const std::string& path, LoadCallback callback);
};

} // namespace assets
} // namespace eyelib
