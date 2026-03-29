#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <memory>

namespace eyelib {
namespace render {

class LightCullPass;

/**
 * Owns LightCullPass lifetime (Forward+ clustered light culling compute).
 * Scene buffers (point/spot lists, light index SSBO) and ClusteredLightGrid stay on RenderPipeline.
 */
class LightCullPassSystem {
public:
    LightCullPassSystem() = default;
    ~LightCullPassSystem();

    LightCullPassSystem(const LightCullPassSystem&) = delete;
    LightCullPassSystem& operator=(const LightCullPassSystem&) = delete;

    bool Initialize(rhi::IDevice* device);
    void Shutdown();

    LightCullPass* GetPass() const { return m_lightCullPass.get(); }
    bool IsSupported(rhi::IDevice* device) const;

private:
    rhi::IDevice* m_device = nullptr;
    std::unique_ptr<LightCullPass> m_lightCullPass;
};

}  // namespace render
}  // namespace eyelib
