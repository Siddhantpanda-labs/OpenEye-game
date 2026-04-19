#pragma once

#include "eyelib/rendering/shaders/ShaderPermutationKey.h"
#include "eyelib/rendering/rhi/RHITypes.h"

#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {
struct IDevice;
struct IPipeline;
}

namespace shaders {

/**
 * Declared permutation specs (filled at feature `OnRegister`) + one-time GPU pipeline warm-up (B.2).
 * Execute paths must only bind `TryGetPipeline` results — never `CreateGraphicsPipelineFromShaderAsset` (D7).
 */
class ShaderLibrary {
public:
    void ClearRegistrations() noexcept { m_entries.clear(); }

    void AddEntry(ShaderPermutationKey key, rhi::ShaderGraphicsPipelineCreateInfo spec);

    /** Idempotent: compiles any entry with a null pipeline. */
    void WarmUpPipelines(rhi::IDevice* device) noexcept;

    void DestroyPipelines(rhi::IDevice* device) noexcept;

    rhi::IPipeline* TryGetPipeline(ShaderPermutationKey key) const noexcept;

    size_t EntryCount() const noexcept { return m_entries.size(); }

private:
    struct Entry {
        ShaderPermutationKey key{};
        rhi::ShaderGraphicsPipelineCreateInfo spec{};
        rhi::IPipeline* pipeline = nullptr;
    };
    std::vector<Entry> m_entries;
};

}  // namespace shaders
}  // namespace rendering
}  // namespace eyelib
