#pragma once

#include "eyelib/render/ShaderDescriptor.h"
#include "eyelib/render/rhi/RHI.h"

#include <memory>
#include <string>

namespace eyelib {
namespace render {

class Shader;

/**
 * Loads shaders by name or paths. Engine/runtime only (not a user asset).
 * No AssetId or asset registry; callers register with their registry if needed.
 */
class ShaderLoader {
public:
    /** Build descriptor for loading by logical name (e.g. passes/forward/forward). */
    static ShaderDescriptor BuildDescriptorByName(const std::string& shaderName);

    /** Build descriptor for loading by vertex/fragment paths. */
    static ShaderDescriptor BuildDescriptorFromPaths(const std::string& vertexPath, const std::string& fragmentPath);

    /** Load shader by name. Returns null on failure. */
    static std::shared_ptr<Shader> LoadShader(rhi::IDevice* device, const std::string& shaderName);

    /** Load shader by vertex/fragment paths. Returns null on failure. */
    static std::shared_ptr<Shader> LoadShader(rhi::IDevice* device, const std::string& vertexPath, const std::string& fragmentPath);
};

} // namespace render
} // namespace eyelib
