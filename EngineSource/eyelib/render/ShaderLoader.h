#pragma once

#include "eyelib/render/ShaderDescriptor.h"
#include "eyelib/render/rhi/RHI.h"
#include <string>

namespace eyelib {
namespace render {

/**
 * Loads shaders by name or paths. Engine/runtime only (not a user asset).
 * No AssetId or asset registry; callers register with their registry if needed.
 */
class ShaderLoader {
public:
    /** Build descriptor for loading by logical shader asset path (e.g. passes/forward/forward). */
    static ShaderDescriptor BuildDescriptorFromAsset(const std::string& assetPath);

    /** Build descriptor for loading by vertex/fragment paths. */
    static ShaderDescriptor BuildDescriptorFromPaths(const std::string& vertexPath, const std::string& fragmentPath);

};

} // namespace render
} // namespace eyelib
