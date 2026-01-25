#include "api/render/ShadowMap.h"
#include "eyelib/render/ShadowMap.h"

namespace eyelib {
namespace api {

class ShadowMap::Impl {
public:
    std::shared_ptr<render::ShadowMap> shadow = std::make_shared<render::ShadowMap>();
};

ShadowMap::ShadowMap() : m_impl(std::make_unique<Impl>()) {}

ShadowMap::~ShadowMap() = default;

ShadowMap::ShadowMap(ShadowMap&& other) noexcept = default;

ShadowMap& ShadowMap::operator=(ShadowMap&& other) noexcept = default;

bool ShadowMap::Initialize(uint32_t resolution) {
    return m_impl->shadow->Initialize(resolution);
}

void ShadowMap::UpdateMatrices(const math::Vec3& lightDir, const math::Mat4& view, const math::Mat4& proj,
                               const math::Vec3& cameraPos, float nearPlane, float farPlane) {
    m_impl->shadow->UpdateMatrices(lightDir, view, proj, cameraPos, nearPlane, farPlane);
}

void ShadowMap::BindForShadowPass() {
    m_impl->shadow->BindForShadowPass();
}

void ShadowMap::Unbind() {
    m_impl->shadow->Unbind();
}

uint32_t ShadowMap::GetDepthTexture() const {
    return static_cast<uint32_t>(m_impl->shadow->GetDepthTexture());
}

const math::Mat4& ShadowMap::GetShadowMatrix() const {
    return m_impl->shadow->GetShadowMatrix();
}

uint32_t ShadowMap::GetResolution() const {
    return m_impl->shadow->GetResolution();
}

std::shared_ptr<render::ShadowMap> ShadowMap::GetInternal() const {
    return m_impl->shadow;
}

std::shared_ptr<ShadowMap> ShadowMap::WrapInternal(const std::shared_ptr<render::ShadowMap>& shadow) {
    auto apiShadow = std::make_shared<ShadowMap>();
    if (shadow) apiShadow->m_impl->shadow = shadow;
    return apiShadow;
}

} // namespace api
} // namespace eyelib
