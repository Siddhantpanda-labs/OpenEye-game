#include "api/render/MaterialInstance.h"
#include "api/render/Material.h"
#include "api/render/Texture.h"
#include "eyelib/render/resources/MaterialInstance.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Texture2D.h"

namespace eyelib {
namespace api {

class MaterialInstance::Impl {
public:
    std::shared_ptr<render::MaterialInstance> instance;
};

MaterialInstance::MaterialInstance() : m_impl(std::make_unique<Impl>()) {}

MaterialInstance::MaterialInstance(std::shared_ptr<Material> material) : m_impl(std::make_unique<Impl>()) {
    if (material) {
        m_impl->instance = std::make_shared<render::MaterialInstance>(material->GetInternal());
    }
}

MaterialInstance::~MaterialInstance() = default;

MaterialInstance::MaterialInstance(MaterialInstance&& other) noexcept = default;

MaterialInstance& MaterialInstance::operator=(MaterialInstance&& other) noexcept = default;

void MaterialInstance::SetMaterial(std::shared_ptr<Material> material) {
    if (!material) return;
    m_impl->instance = std::make_shared<render::MaterialInstance>(material->GetInternal());
}

std::shared_ptr<Material> MaterialInstance::GetMaterial() const {
    if (!m_impl->instance) return nullptr;
    return Material::WrapInternal(m_impl->instance->GetMaterial());
}

void MaterialInstance::SetTextureOverride(const std::string& name, std::shared_ptr<Texture> texture) {
    if (!m_impl->instance || !texture) return;
    m_impl->instance->SetTextureOverride(name, texture->GetInternal());
}

void MaterialInstance::SetAlbedoOverride(const math::Vec3& color) {
    if (!m_impl->instance) return;
    m_impl->instance->SetAlbedoOverride(color);
}

void MaterialInstance::SetMetallicOverride(float value) {
    if (!m_impl->instance) return;
    m_impl->instance->SetMetallicOverride(value);
}

void MaterialInstance::SetRoughnessOverride(float value) {
    if (!m_impl->instance) return;
    m_impl->instance->SetRoughnessOverride(value);
}

void MaterialInstance::SetEmissionOverride(const math::Vec3& color) {
    if (!m_impl->instance) return;
    m_impl->instance->SetEmissionOverride(color);
}

void MaterialInstance::SetAlphaOverride(float value) {
    if (!m_impl->instance) return;
    m_impl->instance->SetAlphaOverride(value);
}

void MaterialInstance::SetFloatOverride(const std::string& name, float value) {
    if (!m_impl->instance) return;
    m_impl->instance->SetFloatOverride(name, value);
}

void MaterialInstance::SetVec3Override(const std::string& name, const math::Vec3& value) {
    if (!m_impl->instance) return;
    m_impl->instance->SetVec3Override(name, value);
}

void MaterialInstance::SetVec4Override(const std::string& name, const math::Vec4& value) {
    if (!m_impl->instance) return;
    m_impl->instance->SetVec4Override(name, value);
}

void MaterialInstance::ClearOverrides() {
    if (!m_impl->instance) return;
    m_impl->instance->ClearOverrides();
}

std::shared_ptr<render::MaterialInstance> MaterialInstance::GetInternal() const {
    return m_impl->instance;
}

std::shared_ptr<MaterialInstance> MaterialInstance::WrapInternal(const std::shared_ptr<render::MaterialInstance>& instance) {
    auto apiInstance = std::make_shared<MaterialInstance>();
    if (instance) apiInstance->m_impl->instance = instance;
    return apiInstance;
}

} // namespace api
} // namespace eyelib
