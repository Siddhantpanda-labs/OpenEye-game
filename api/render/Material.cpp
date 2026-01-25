#include "api/render/Material.h"
#include "api/render/Texture.h"
#include "api/render/Shader.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Texture2D.h"

namespace eyelib {
namespace api {

class Material::Impl {
public:
    std::shared_ptr<::eyelib::render::Material> material = std::make_shared<::eyelib::render::Material>();
};

Material::Material() : m_impl(std::make_unique<Impl>()) {}

Material::~Material() = default;

Material::Material(Material&& other) noexcept = default;

Material& Material::operator=(Material&& other) noexcept = default;

void Material::SetTexture(const std::string& name, std::shared_ptr<Texture> texture) {
    if (!texture) return;
    auto texImpl = texture->GetInternal();
    m_impl->material->SetTexture(name, texImpl);
}

void Material::SetShader(std::shared_ptr<Shader> shader) {
    if (!shader) return;
    m_impl->material->SetShader(shader->GetInternal());
}

void Material::SetAlbedo(const math::Vec3& color) {
    m_impl->material->SetAlbedo(color);
}

void Material::SetMetallic(float value) {
    m_impl->material->SetMetallic(value);
}

void Material::SetRoughness(float value) {
    m_impl->material->SetRoughness(value);
}

void Material::SetEmission(const math::Vec3& color) {
    m_impl->material->SetEmission(color);
}

void Material::SetAO(float value) {
    m_impl->material->SetAO(value);
}

void Material::SetAlpha(float value) {
    m_impl->material->SetAlpha(value);
}

math::Vec3 Material::GetAlbedo() const {
    return m_impl->material->GetAlbedo();
}

float Material::GetMetallic() const {
    return m_impl->material->GetMetallic();
}

float Material::GetRoughness() const {
    return m_impl->material->GetRoughness();
}

math::Vec3 Material::GetEmission() const {
    return m_impl->material->GetEmission();
}

float Material::GetAO() const {
    return m_impl->material->GetAO();
}

float Material::GetAlpha() const {
    return m_impl->material->GetAlpha();
}

void Material::SetAlbedoTexture(std::shared_ptr<Texture> t) {
    SetTexture("u_AlbedoTexture", t);
}

void Material::SetNormalTexture(std::shared_ptr<Texture> t) {
    SetTexture("u_NormalTexture", t);
}

void Material::SetMetallicTexture(std::shared_ptr<Texture> t) {
    SetTexture("u_MetallicTexture", t);
}

void Material::SetRoughnessTexture(std::shared_ptr<Texture> t) {
    SetTexture("u_RoughnessTexture", t);
}

void Material::SetAOTexture(std::shared_ptr<Texture> t) {
    SetTexture("u_AOTexture", t);
}

void Material::SetEmissiveTexture(std::shared_ptr<Texture> t) {
    SetTexture("u_EmissiveTexture", t);
}

void Material::SetMetallicRoughnessTexture(std::shared_ptr<Texture> t) {
    SetTexture("u_MetallicRoughnessTexture", t);
}

void Material::SetFloat(const std::string& name, float value) {
    m_impl->material->SetFloat(name, value);
}

void Material::SetVec3(const std::string& name, const math::Vec3& value) {
    m_impl->material->SetVec3(name, value);
}

void Material::SetVec4(const std::string& name, const math::Vec4& value) {
    m_impl->material->SetVec4(name, value);
}

float Material::GetFloat(const std::string& name, float defaultValue) const {
    return m_impl->material->GetFloat(name, defaultValue);
}

math::Vec3 Material::GetVec3(const std::string& name, const math::Vec3& defaultValue) const {
    return m_impl->material->GetVec3(name, defaultValue);
}

math::Vec4 Material::GetVec4(const std::string& name, const math::Vec4& defaultValue) const {
    return m_impl->material->GetVec4(name, defaultValue);
}

void Material::SetKeyword(const std::string& keyword, bool enabled) {
    m_impl->material->SetKeyword(keyword, enabled);
}

bool Material::IsKeywordEnabled(const std::string& keyword) const {
    return m_impl->material->IsKeywordEnabled(keyword);
}

std::shared_ptr<Material> Material::WrapInternal(const std::shared_ptr<::eyelib::render::Material>& material) {
    auto apiMaterial = std::make_shared<Material>();
    if (material) apiMaterial->m_impl->material = material;
    return apiMaterial;
}

std::shared_ptr<::eyelib::render::Material> Material::GetInternal() const {
    return m_impl->material;
}

} // namespace api
} // namespace eyelib
