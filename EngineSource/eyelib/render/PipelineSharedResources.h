#pragma once

#include <memory>

namespace eyelib {
namespace render {

class ShaderCache;
class Shader;

/**
 * Resources shared between pipelines using the same RHI device.
 * First pipeline populates; subsequent pipelines reuse to avoid duplicate shader compilation.
 */
struct PipelineSharedResources {
    std::unique_ptr<ShaderCache> shaderCache;
    std::shared_ptr<Shader> skyboxShader;
    std::shared_ptr<Shader> proceduralSkyboxShader;
    std::shared_ptr<Shader> stereoForwardShader;
    std::shared_ptr<Shader> outlinePostShader;
    std::shared_ptr<Shader> entityIdShader;
};

} // namespace render
} // namespace eyelib
