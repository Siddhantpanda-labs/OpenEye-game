#pragma once

#include "eyelib/render/ShaderKey.h"
#include <memory>
#include <string>

namespace eyelib {
namespace render {

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
     */
    std::shared_ptr<Shader> compile(rhi::IDevice* device, const Material* material,
                                    PipelineStage stage, InstanceType instanceType);
};

} // namespace render
} // namespace eyelib
