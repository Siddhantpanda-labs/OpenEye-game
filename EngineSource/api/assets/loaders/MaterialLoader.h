#pragma once

#include "api/assets/AssetId.h"

#include <memory>
#include <string>

namespace eyelib {
namespace api {

class MaterialLoader {
public:
    /** Load material from .mat JSON file. Returns AssetId or Invalid. */
    static AssetId LoadMaterial(const std::string& path);

    /** Save material to .mat JSON (with texture asset IDs). Uses asset cache lookup by id. Returns true on success. */
    static bool SaveMaterial(const std::string& path, const AssetId& materialAssetId);
};

} // namespace api
} // namespace eyelib
