#pragma once

#include "eyelib/platform/filesystem/FileSystem.h"

#include <string>

namespace eyelib {
namespace rendering {
namespace shaders {

/** Relative to repo / runtime engine root: Vulkan sources and SPIR-V for `eyelib::rendering` only. */
inline constexpr const char kVulkanRenderingShaderRoot[] = "assets/shaders/rendering/vulkan";

/** Prefix for `IDevice::ResolveShaderPath` (relative path, trailing slash). */
inline constexpr const char kVulkanRenderingShaderPathPrefix[] = "assets/shaders/rendering/vulkan/";

/** Base directory for GLSL/SPIR-V (trailing `/`). Uses `GetEngineAssetsRoot()` when set. */
inline std::string VulkanRenderingShaderBaseDir() {
    std::string root = platform::filesystem::GetEngineAssetsRoot();
    if (root.empty())
        return std::string(kVulkanRenderingShaderPathPrefix);
    return platform::filesystem::NormalizePath(root + "/" + std::string(kVulkanRenderingShaderRoot) + "/");
}

}  // namespace shaders
}  // namespace rendering
}  // namespace eyelib
