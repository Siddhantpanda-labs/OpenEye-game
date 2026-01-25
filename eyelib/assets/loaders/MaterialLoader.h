#pragma once

#include "eyelib/assets/AssetId.h"

#include <memory>
#include <string>

namespace eyelib {
namespace render {
class Material;
}

namespace assets {

class MaterialLoader {
public:
    /** Load material from .mat JSON file. Returns AssetId or Invalid. */
    static AssetId LoadMaterial(const std::string& path);

    /** Save material to .mat JSON with texture asset IDs. Returns true on success. */
    static bool SaveMaterial(const std::string& path, const std::shared_ptr<render::Material>& material);
};

} // namespace assets
} // namespace eyelib
