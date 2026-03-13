#include "api/render/CascadeShadowMap.h"
#include "api/render/RenderDevice.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace api {

class CascadeShadowMap::Impl {
public:
    std::shared_ptr<::eyelib::render::CascadeShadowMap> owned;
    ::eyelib::render::CascadeShadowMap* view = nullptr;

    ::eyelib::render::CascadeShadowMap* get() const {
        return view ? view : owned.get();
    }
};

CascadeShadowMap::CascadeShadowMap() : m_impl(std::make_unique<Impl>()) {}

CascadeShadowMap::~CascadeShadowMap() = default;

CascadeShadowMap::CascadeShadowMap(CascadeShadowMap&& other) noexcept = default;

CascadeShadowMap& CascadeShadowMap::operator=(CascadeShadowMap&& other) noexcept = default;

bool CascadeShadowMap::Initialize(RenderDevice device, uint32_t resolution, uint32_t splitCount) {
    if (!device) return false;
    auto* rhi = static_cast<::eyelib::render::rhi::IDevice*>(device.Get());
    m_impl->owned = std::make_shared<::eyelib::render::CascadeShadowMap>();
    m_impl->view = nullptr;
    return m_impl->owned->Initialize(rhi, resolution, splitCount);
}

const math::Mat4& CascadeShadowMap::GetLightSpaceMatrix(uint32_t cascadeIndex) const {
    auto* csm = m_impl->get();
    static const math::Mat4 kIdentity(1.0f);
    return csm ? csm->GetLightSpaceMatrix(cascadeIndex) : kIdentity;
}

uint32_t CascadeShadowMap::GetResolution() const {
    auto* csm = m_impl->get();
    return csm ? csm->GetResolution() : 0u;
}

uint32_t CascadeShadowMap::GetSplitCount() const {
    auto* csm = m_impl->get();
    return csm ? csm->GetSplitCount() : 0u;
}

::eyelib::render::CascadeShadowMap* CascadeShadowMap::GetInternal() const {
    return m_impl->get();
}

std::shared_ptr<CascadeShadowMap> CascadeShadowMap::WrapInternal(::eyelib::render::CascadeShadowMap* csm) {
    auto apiCsm = std::make_shared<CascadeShadowMap>();
    if (apiCsm->m_impl && csm) {
        apiCsm->m_impl->view = csm;
        apiCsm->m_impl->owned.reset();
    }
    return apiCsm;
}

} // namespace api
} // namespace eyelib
