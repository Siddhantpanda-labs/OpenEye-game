#include "eyelib/render/reflection/ReflectionProbe.h"
#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {

void ReflectionProbe::BindForLighting(rhi::ICommandList* cmd, unsigned int irradianceUnit,
                                      unsigned int prefilterUnit, unsigned int brdfLUTUnit,
                                      rhi::ITexture* brdfLUT) const {
    if (!cmd) return;
    if (m_irradianceMap) cmd->BindTexture(m_irradianceMap, irradianceUnit);
    if (m_prefilteredMap) cmd->BindTexture(m_prefilteredMap, prefilterUnit);
    if (brdfLUT) cmd->BindTexture(brdfLUT, brdfLUTUnit);
}

} // namespace render
} // namespace eyelib
