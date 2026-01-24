#include "eyelib/render/resources/MaterialInstance.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"

namespace eyelib {
namespace render {

MaterialInstance::MaterialInstance(std::shared_ptr<Material> material) : m_base(std::move(material)) {}

void MaterialInstance::SetTextureOverride(const std::string& name, std::shared_ptr<Texture2D> texture) {
    m_textureOverrides[name] = std::move(texture);
}

void MaterialInstance::SetAlbedoOverride(const math::Vec3& color) { m_albedoOverride = color; }
void MaterialInstance::SetMetallicOverride(float v) { m_metallicOverride = v; }
void MaterialInstance::SetRoughnessOverride(float v) { m_roughnessOverride = v; }
void MaterialInstance::SetEmissionOverride(const math::Vec3& color) { m_emissionOverride = color; }
void MaterialInstance::SetAlphaOverride(float v) { m_alphaOverride = v; }
void MaterialInstance::SetFloatOverride(const std::string& name, float value) { m_floatOverrides[name] = value; }
void MaterialInstance::SetVec3Override(const std::string& name, const math::Vec3& value) { m_vec3Overrides[name] = value; }
void MaterialInstance::SetVec4Override(const std::string& name, const math::Vec4& value) { m_vec4Overrides[name] = value; }

void MaterialInstance::ClearOverrides() {
    m_textureOverrides.clear();
    m_albedoOverride.reset();
    m_metallicOverride.reset();
    m_roughnessOverride.reset();
    m_emissionOverride.reset();
    m_alphaOverride.reset();
    m_floatOverrides.clear();
    m_vec3Overrides.clear();
    m_vec4Overrides.clear();
}

void MaterialInstance::Bind() const {
    if (!m_base || !m_base->GetShader() || !m_base->GetShader()->IsValid()) return;

    Shader* sh = m_base->GetShader().get();
    sh->Bind();

    int unit = 0;
    m_base->ForEachTexture([this, sh, &unit](const std::string& name, const std::shared_ptr<Texture2D>& baseTex) {
        std::shared_ptr<Texture2D> t;
        auto it = m_textureOverrides.find(name);
        if (it != m_textureOverrides.end()) t = it->second;
        else t = baseTex;
        if (t && t->IsValid()) {
            t->Bind(unit);
            sh->SetInt(name, unit);
            ++unit;
        }
    });

    math::Vec3 albedo = m_albedoOverride ? *m_albedoOverride : m_base->GetAlbedo();
    float metallic = m_metallicOverride ? *m_metallicOverride : m_base->GetMetallic();
    float roughness = m_roughnessOverride ? *m_roughnessOverride : m_base->GetRoughness();
    math::Vec3 emission = m_emissionOverride ? *m_emissionOverride : m_base->GetEmission();
    float alpha = m_alphaOverride ? *m_alphaOverride : m_base->GetAlpha();

    sh->SetVec3("u_Albedo", albedo);
    sh->SetFloat("u_Metallic", metallic);
    sh->SetFloat("u_Roughness", roughness);
    sh->SetVec3("u_Emission", emission);
    sh->SetFloat("u_AO", m_base->GetAO());
    sh->SetFloat("u_Alpha", alpha);

    m_base->ForEachFloat([sh](const std::string& n, float v) { sh->SetFloat(n, v); });
    m_base->ForEachVec3([sh](const std::string& n, const math::Vec3& v) { sh->SetVec3(n, v); });
    m_base->ForEachVec4([sh](const std::string& n, const math::Vec4& v) { sh->SetVec4(n, v); });
    for (const auto& p : m_floatOverrides) sh->SetFloat(p.first, p.second);
    for (const auto& p : m_vec3Overrides) sh->SetVec3(p.first, p.second);
    for (const auto& p : m_vec4Overrides) sh->SetVec4(p.first, p.second);
}

} // namespace render
} // namespace eyelib
