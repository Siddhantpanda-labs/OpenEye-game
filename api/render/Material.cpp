#include "api/render/Material.h"
#include "api/render/Texture.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Texture2D.h"

namespace eyelib {
namespace api {

class Material::Impl {
public:
    std::shared_ptr<render::Material> material = std::make_shared<render::Material>();
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

std::shared_ptr<Material> Material::WrapInternal(const std::shared_ptr<render::Material>& material) {
    auto apiMaterial = std::make_shared<Material>();
    if (material) apiMaterial->m_impl->material = material;
    return apiMaterial;
}

std::shared_ptr<render::Material> Material::GetInternal() const {
    return m_impl->material;
}

} // namespace api
} // namespace eyelib
