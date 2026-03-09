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
    std::shared_ptr<::eyelib::render::MaterialInstance> instance;
};

MaterialInstance::MaterialInstance() : m_impl(std::make_unique<Impl>()) {}

MaterialInstance::MaterialInstance(std::shared_ptr<Material> material) : m_impl(std::make_unique<Impl>()) {
    if (material) {
        m_impl->instance = std::make_shared<::eyelib::render::MaterialInstance>(material->GetInternal());
    }
}

MaterialInstance::~MaterialInstance() = default;

MaterialInstance::MaterialInstance(MaterialInstance&& other) noexcept = default;

MaterialInstance& MaterialInstance::operator=(MaterialInstance&& other) noexcept = default;

void MaterialInstance::SetMaterial(std::shared_ptr<Material> material) {
    if (!material) return;
    m_impl->instance = std::make_shared<::eyelib::render::MaterialInstance>(material->GetInternal());
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

void MaterialInstance::SetMat4ArrayOverride(const std::string& name, const std::vector<math::Mat4>& values) {
    if (!m_impl->instance) return;
    m_impl->instance->SetMat4ArrayOverride(name, values);
}

void MaterialInstance::ClearOverrides() {
    if (!m_impl->instance) return;
    m_impl->instance->ClearOverrides();
}

std::shared_ptr<Texture> MaterialInstance::GetTexture(const std::string& name) const {
    if (!m_impl->instance) return nullptr;
    return Texture::WrapInternal(m_impl->instance->GetTexture(name));
}

math::Vec3 MaterialInstance::GetAlbedo() const {
    return m_impl->instance ? m_impl->instance->GetAlbedo() : math::Vec3(0.0f);
}

float MaterialInstance::GetMetallic() const {
    return m_impl->instance ? m_impl->instance->GetMetallic() : 0.0f;
}

float MaterialInstance::GetRoughness() const {
    return m_impl->instance ? m_impl->instance->GetRoughness() : 0.0f;
}

math::Vec3 MaterialInstance::GetEmission() const {
    return m_impl->instance ? m_impl->instance->GetEmission() : math::Vec3(0.0f);
}

float MaterialInstance::GetAO() const {
    return m_impl->instance ? m_impl->instance->GetAO() : 1.0f;
}

float MaterialInstance::GetAlpha() const {
    return m_impl->instance ? m_impl->instance->GetAlpha() : 1.0f;
}

float MaterialInstance::GetFloat(const std::string& name, float defaultValue) const {
    return m_impl->instance ? m_impl->instance->GetFloat(name, defaultValue) : defaultValue;
}

math::Vec3 MaterialInstance::GetVec3(const std::string& name, const math::Vec3& defaultValue) const {
    return m_impl->instance ? m_impl->instance->GetVec3(name, defaultValue) : defaultValue;
}

math::Vec4 MaterialInstance::GetVec4(const std::string& name, const math::Vec4& defaultValue) const {
    return m_impl->instance ? m_impl->instance->GetVec4(name, defaultValue) : defaultValue;
}

void MaterialInstance::ForEachMat4Array(std::function<void(const std::string&, const math::Mat4*, size_t)> fn) const {
    if (!m_impl->instance || !fn) return;
    m_impl->instance->ForEachMat4Array(std::move(fn));
}

std::shared_ptr<::eyelib::render::MaterialInstance> MaterialInstance::GetInternal() const {
    return m_impl->instance;
}

std::shared_ptr<MaterialInstance> MaterialInstance::WrapInternal(const std::shared_ptr<::eyelib::render::MaterialInstance>& instance) {
    auto apiInstance = std::make_shared<MaterialInstance>();
    if (instance) apiInstance->m_impl->instance = instance;
    return apiInstance;
}

} // namespace api
} // namespace eyelib
