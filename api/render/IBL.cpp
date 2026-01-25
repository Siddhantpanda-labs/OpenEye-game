#include "api/render/IBL.h"
#include "eyelib/render/IBL.h"
#include "eyelib/render/resources/TextureCube.h"

namespace eyelib {
namespace api {

class IBL::Impl {
public:
    std::unique_ptr<render::IBL> ibl = std::make_unique<render::IBL>();
};

IBL::IBL() : m_impl(std::make_unique<Impl>()) {}

IBL::~IBL() = default;

IBL::IBL(IBL&& other) noexcept = default;

IBL& IBL::operator=(IBL&& other) noexcept = default;

bool IBL::Initialize(const TextureCube& environment, uint32_t irradianceRes, uint32_t prefilterRes) {
    auto internal = environment.GetInternal();
    if (!internal) return false;
    return m_impl->ibl->Initialize(internal->Id(), irradianceRes, prefilterRes);
}

void IBL::BindForLighting(uint32_t irradianceUnit, uint32_t prefilterUnit, uint32_t brdfLutUnit) const {
    m_impl->ibl->BindForLighting(irradianceUnit, prefilterUnit, brdfLutUnit);
}

uint32_t IBL::GetIrradianceMap() const {
    return static_cast<uint32_t>(m_impl->ibl->GetIrradianceMap());
}

uint32_t IBL::GetPrefilterMap() const {
    return static_cast<uint32_t>(m_impl->ibl->GetPrefilterMap());
}

uint32_t IBL::GetBRDFLUT() const {
    return static_cast<uint32_t>(m_impl->ibl->GetBRDFLUT());
}

bool IBL::IsValid() const {
    return m_impl->ibl->IsValid();
}

} // namespace api
} // namespace eyelib
