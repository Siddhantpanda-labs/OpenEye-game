#pragma once

#include "api/asset/AssetId.h"

#include <memory>
#include <string>

namespace eyelib { namespace api { class AssetRegistry; } }

namespace eyelib {
namespace api {

class MaterialLoader {
public:
    /** Load material via registry (canonical/asset). Returns AssetId or Invalid. */
    static AssetId LoadMaterial(AssetRegistry* registry, const std::string& path);
    /** No registry: returns Invalid. */
    static AssetId LoadMaterial(const std::string& path);

    /** Save material to canonical .mat binary. Stub: returns false. */
    static bool SaveMaterial(const std::string& path, const AssetId& materialAssetId);
};

} // namespace api
} // namespace eyelib
