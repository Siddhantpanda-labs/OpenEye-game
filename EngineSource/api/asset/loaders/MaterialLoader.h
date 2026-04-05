#pragma once

#include "api/asset/AssetId.h"

#include <memory>
#include <string>

namespace eyelib { namespace api { class AssetRegistry; } }
namespace eyelib { namespace render { struct MaterialDescriptor; } }

namespace eyelib {
namespace api {

class MaterialLoader {
public:
    /** Load material via registry (canonical/asset). Returns AssetId or Invalid. */
    static AssetId LoadMaterial(AssetRegistry* registry, const std::string& path);
    /** No registry: returns Invalid. */
    static AssetId LoadMaterial(const std::string& path);

    /** Save CPU descriptor to MAT1 v3 binary at \a absolutePath (e.g. project Assets + relative id). */
    static bool SaveMaterial(const std::string& absolutePath, const ::eyelib::render::MaterialDescriptor& desc);
};

} // namespace api
} // namespace eyelib
