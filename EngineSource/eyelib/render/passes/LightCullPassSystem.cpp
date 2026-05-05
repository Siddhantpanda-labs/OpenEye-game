#include "eyelib/render/passes/LightCullPassSystem.h"
#include "eyelib/render/passes/detail/LightCullPass.h"

namespace eyelib {
namespace render {

LightCullPassSystem::~LightCullPassSystem() {
    Shutdown();
}

bool LightCullPassSystem::Initialize(rhi::IDevice* device) {
    Shutdown();
    if (!device)
        return false;
    m_device = device;
    m_lightCullPass = std::make_unique<LightCullPass>();
    if (m_lightCullPass->IsSupported(m_device))
        m_lightCullPass->Initialize(m_device);
    return true;
}

void LightCullPassSystem::Shutdown() {
    if (m_lightCullPass && m_device)
        m_lightCullPass->Shutdown();
    m_lightCullPass.reset();
    m_device = nullptr;
}

bool LightCullPassSystem::IsSupported(rhi::IDevice* device) const {
    return m_lightCullPass && m_lightCullPass->IsSupported(device);
}

}  // namespace render
}  // namespace eyelib
