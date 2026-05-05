#include "eyelib/rendering/shaders/ShaderLibrary.h"

#include "eyelib/core/Log.h"
#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {
namespace shaders {

void ShaderLibrary::AddEntry(ShaderPermutationKey key, rhi::ShaderGraphicsPipelineCreateInfo spec) {
    Entry e{};
    e.key = key;
    e.spec = std::move(spec);
    m_entries.push_back(std::move(e));
}

void ShaderLibrary::WarmUpPipelines(rhi::IDevice* device) noexcept {
    if (!device)
        return;
    for (Entry& e : m_entries) {
        if (!e.pipeline) {
            e.pipeline = device->CreateGraphicsPipelineFromShaderAsset(e.spec);
            if (!e.pipeline && !e.warmUpFailureLogged) {
                e.warmUpFailureLogged = true;
                core::Log::WarnF(
                    "ShaderLibrary::WarmUpPipelines: CreateGraphicsPipelineFromShaderAsset returned null for assetPath='%s' "
                    "(program=%u, msaa=%u, lights=%u).",
                    e.spec.assetPath.c_str(),
                    static_cast<unsigned>(e.key.program),
                    static_cast<unsigned>(e.key.msaaSamples),
                    static_cast<unsigned>(e.key.directionalLightCount));
            }
        }
    }
}

void ShaderLibrary::DestroyPipelines(rhi::IDevice* device) noexcept {
    if (!device)
        return;
    for (Entry& e : m_entries) {
        if (e.pipeline) {
            device->DestroyPipeline(e.pipeline);
            e.pipeline = nullptr;
        }
    }
}

rhi::IPipeline* ShaderLibrary::TryGetPipeline(ShaderPermutationKey key) const noexcept {
    for (const Entry& e : m_entries) {
        if (e.key == key)
            return e.pipeline;
    }
    return nullptr;
}

}  // namespace shaders
}  // namespace rendering
}  // namespace eyelib
