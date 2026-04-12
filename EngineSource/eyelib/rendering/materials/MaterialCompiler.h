#pragma once

#include "eyelib/rendering/shaders/ShaderKey.h"
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace rendering {

namespace rhi { struct IDevice; }
class Material;
class Shader;

/**
 * Compiles a pass shader from the pipeline template by replacing material placeholders.
 * Used when ShaderKey.materialTypeId != kDefaultMaterialTypeId; result is stored in ShaderCache.
 */
class MaterialCompiler {
public:
    MaterialCompiler() = default;

    /**
     * Load pass template for (stage, instanceType), replace @MATERIAL_DEFINES@, @MATERIAL_FRAGMENT_SNIPPET@,
     * @MATERIAL_SHADING_SNIPPET@ from material, compile via RHI, return shared shader. Returns null on failure.
     * If outDependencies is non-null, fills it with all shader file paths used (templates + includes) for hot-reload.
     */
    std::shared_ptr<Shader> compile(rhi::IDevice* device, const Material* material,
                                    PipelineStage stage, InstanceType instanceType,
                                    std::vector<std::string>* outDependencies = nullptr);
};

} // namespace rendering
} // namespace eyelib
