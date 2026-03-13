#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/rhi/RHITypes.h"

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
void MaterialInstance::SetMat4ArrayOverride(const std::string& name, const std::vector<math::Mat4>& values) {
    m_mat4ArrayOverrides[name] = values;
}

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
    m_mat4ArrayOverrides.clear();
}

std::shared_ptr<Texture2D> MaterialInstance::GetTexture(const std::string& name) const {
    auto it = m_textureOverrides.find(name);
    if (it != m_textureOverrides.end()) {
        return it->second;
    }
    return m_base ? m_base->GetTexture(name) : nullptr;
}

rhi::ITexture* MaterialInstance::GetTextureRHI(const std::string& name) const {
    std::shared_ptr<Texture2D> tex = GetTexture(name);
    return (tex && tex->IsValid()) ? tex->GetTextureRHI() : nullptr;
}

bool MaterialInstance::BindTextureToSlot(const std::string& name, int unit) const {
    std::shared_ptr<Texture2D> tex = GetTexture(name);
    if (!tex || !tex->IsValid()) return false;
    tex->Bind(unit);
    return true;
}

math::Vec3 MaterialInstance::GetAlbedo() const {
    return m_albedoOverride ? *m_albedoOverride : (m_base ? m_base->GetAlbedo() : math::Vec3(0.0f));
}

float MaterialInstance::GetMetallic() const {
    return m_metallicOverride ? *m_metallicOverride : (m_base ? m_base->GetMetallic() : 0.0f);
}

float MaterialInstance::GetRoughness() const {
    return m_roughnessOverride ? *m_roughnessOverride : (m_base ? m_base->GetRoughness() : 0.0f);
}

math::Vec3 MaterialInstance::GetEmission() const {
    return m_emissionOverride ? *m_emissionOverride : (m_base ? m_base->GetEmission() : math::Vec3(0.0f));
}

float MaterialInstance::GetAO() const {
    return m_base ? m_base->GetAO() : 1.0f;
}

float MaterialInstance::GetAlpha() const {
    return m_alphaOverride ? *m_alphaOverride : (m_base ? m_base->GetAlpha() : 1.0f);
}

bool MaterialInstance::GetReflective() const {
    return m_base ? m_base->GetReflective() : false;
}

float MaterialInstance::GetReflectiveStrength() const {
    return m_base ? m_base->GetReflectiveStrength() : 0.0f;
}

float MaterialInstance::GetFloat(const std::string& name, float defaultValue) const {
    auto it = m_floatOverrides.find(name);
    if (it != m_floatOverrides.end()) return it->second;
    return m_base ? m_base->GetFloat(name, defaultValue) : defaultValue;
}

math::Vec3 MaterialInstance::GetVec3(const std::string& name, const math::Vec3& defaultValue) const {
    auto it = m_vec3Overrides.find(name);
    if (it != m_vec3Overrides.end()) return it->second;
    return m_base ? m_base->GetVec3(name, defaultValue) : defaultValue;
}

math::Vec4 MaterialInstance::GetVec4(const std::string& name, const math::Vec4& defaultValue) const {
    auto it = m_vec4Overrides.find(name);
    if (it != m_vec4Overrides.end()) return it->second;
    return m_base ? m_base->GetVec4(name, defaultValue) : defaultValue;
}

void MaterialInstance::ForEachMat4Array(std::function<void(const std::string&, const math::Mat4*, size_t)> fn) const {
    if (!fn) return;
    if (m_base) {
        m_base->ForEachMat4Array(fn);
    }
    for (const auto& p : m_mat4ArrayOverrides) {
        if (!p.second.empty()) {
            fn(p.first, p.second.data(), p.second.size());
        }
    }
}

namespace {
struct MaterialBlockStd140 {
    float albedo[3];
    float pad0;
    float metallic;
    float roughness;
    float ao;
    float alpha;
    float emission[3];
    float reflective;
};
} // namespace

void MaterialInstance::EnsureMaterialBuffer(rhi::IDevice* device) const {
    if (!device) return;
    if (!m_materialBuffer) {
        rhi::BufferDesc desc;
        desc.sizeInBytes = kMaterialBlockStd140Size;
        desc.usage = rhi::BufferUsage::Uniform;
        desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_materialBuffer.reset(device->CreateBuffer(desc, nullptr));
        if (!m_materialBuffer) return;
    }
    MaterialBlockStd140 data;
    math::Vec3 albedo = GetAlbedo();
    data.albedo[0] = albedo.x;
    data.albedo[1] = albedo.y;
    data.albedo[2] = albedo.z;
    data.metallic = GetMetallic();
    data.roughness = GetRoughness();
    data.ao = GetAO();
    data.alpha = GetAlpha();
    math::Vec3 emission = GetEmission();
    data.emission[0] = emission.x;
    data.emission[1] = emission.y;
    data.emission[2] = emission.z;
    data.reflective = m_base ? m_base->GetReflectiveStrength() : 0.0f;
    data.pad0 = 0.0f;
    m_materialBuffer->Update(&data, 0, kMaterialBlockStd140Size);
}

rhi::IBuffer* MaterialInstance::GetMaterialBufferRHI(rhi::IDevice* device) const {
    EnsureMaterialBuffer(device);
    return m_materialBuffer.get();
}

void MaterialInstance::ApplyToShader(rhi::IDevice* device, Shader* shader, rhi::ICommandList* cmd) const {
    if (!device || !shader || !m_base) return;

    if (cmd) {
        rhi::IBuffer* buf = GetMaterialBufferRHI(device);
        if (buf)
            cmd->BindUniformBuffer(buf, rhi::UniformBinding::PerMaterial, 0, static_cast<uint32_t>(kMaterialBlockStd140Size));
    }

    int unit = 0;
    m_base->ForEachTexture([this, shader, &unit](const std::string& name, const std::shared_ptr<Texture2D>& baseTex) {
        std::shared_ptr<Texture2D> t;
        auto it = m_textureOverrides.find(name);
        if (it != m_textureOverrides.end()) t = it->second;
        else t = baseTex;
        if (t && t->IsValid()) {
            t->Bind(unit);
            shader->SetInt(name, unit);
            ++unit;
        }
    });

    // PBR params come from MaterialBlock UBO (binding 2); instance UBO is filled from base + overrides in GetMaterialBufferRHI; pipeline binds it before draw.

    m_base->ForEachFloat([shader](const std::string& n, float v) { shader->SetFloat(n, v); });
    m_base->ForEachVec3([shader](const std::string& n, const math::Vec3& v) { shader->SetVec3(n, v); });
    m_base->ForEachVec4([shader](const std::string& n, const math::Vec4& v) { shader->SetVec4(n, v); });
    for (const auto& p : m_floatOverrides) shader->SetFloat(p.first, p.second);
    for (const auto& p : m_vec3Overrides) shader->SetVec3(p.first, p.second);
    for (const auto& p : m_vec4Overrides) shader->SetVec4(p.first, p.second);
    for (const auto& p : m_mat4ArrayOverrides) {
        if (!p.second.empty())
            shader->SetMat4Array(p.first, p.second.data(), p.second.size());
    }
}

} // namespace render
} // namespace eyelib
