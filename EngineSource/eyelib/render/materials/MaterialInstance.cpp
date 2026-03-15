#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/rhi/RHITypes.h"
#include <algorithm>
#include <vector>

namespace eyelib {
namespace render {

namespace {
template<typename T>
inline void HashCombine(size_t& seed, const T& v) {
    seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template<typename T>
bool CompareScalarMap(const std::unordered_map<std::string, T>& a, const std::unordered_map<std::string, T>& b) {
    if (a.size() != b.size()) return false;
    for (const auto& kv : a) {
        auto it = b.find(kv.first);
        if (it == b.end()) return false;
        if (!(it->second == kv.second)) return false;
    }
    return true;
}

void AppendMat4Hash(size_t& h, const math::Mat4& m) {
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            HashCombine(h, m[r][c]);
}

bool Mat4Equal(const math::Mat4& a, const math::Mat4& b) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (a[r][c] != b[r][c]) return false;
        }
    }
    return true;
}
} // namespace

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

size_t MaterialInstance::GetBatchHash() const {
    size_t h = m_base ? m_base->GetHash() : 0u;

    HashCombine(h, m_albedoOverride.has_value());
    if (m_albedoOverride) {
        HashCombine(h, m_albedoOverride->x);
        HashCombine(h, m_albedoOverride->y);
        HashCombine(h, m_albedoOverride->z);
    }
    HashCombine(h, m_metallicOverride.has_value());
    if (m_metallicOverride) HashCombine(h, *m_metallicOverride);
    HashCombine(h, m_roughnessOverride.has_value());
    if (m_roughnessOverride) HashCombine(h, *m_roughnessOverride);
    HashCombine(h, m_emissionOverride.has_value());
    if (m_emissionOverride) {
        HashCombine(h, m_emissionOverride->x);
        HashCombine(h, m_emissionOverride->y);
        HashCombine(h, m_emissionOverride->z);
    }
    HashCombine(h, m_alphaOverride.has_value());
    if (m_alphaOverride) HashCombine(h, *m_alphaOverride);

    std::vector<std::string> keys;
    keys.reserve(m_textureOverrides.size());
    for (const auto& kv : m_textureOverrides) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    for (const std::string& k : keys) {
        HashCombine(h, k);
        auto it = m_textureOverrides.find(k);
        const Texture2D* t = (it != m_textureOverrides.end() && it->second) ? it->second.get() : nullptr;
        HashCombine(h, reinterpret_cast<uintptr_t>(t));
    }

    keys.clear();
    keys.reserve(m_floatOverrides.size());
    for (const auto& kv : m_floatOverrides) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    for (const std::string& k : keys) {
        HashCombine(h, k);
        HashCombine(h, m_floatOverrides.at(k));
    }

    keys.clear();
    keys.reserve(m_vec3Overrides.size());
    for (const auto& kv : m_vec3Overrides) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    for (const std::string& k : keys) {
        HashCombine(h, k);
        const math::Vec3& v = m_vec3Overrides.at(k);
        HashCombine(h, v.x); HashCombine(h, v.y); HashCombine(h, v.z);
    }

    keys.clear();
    keys.reserve(m_vec4Overrides.size());
    for (const auto& kv : m_vec4Overrides) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    for (const std::string& k : keys) {
        HashCombine(h, k);
        const math::Vec4& v = m_vec4Overrides.at(k);
        HashCombine(h, v.x); HashCombine(h, v.y); HashCombine(h, v.z); HashCombine(h, v.w);
    }

    keys.clear();
    keys.reserve(m_mat4ArrayOverrides.size());
    for (const auto& kv : m_mat4ArrayOverrides) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    for (const std::string& k : keys) {
        HashCombine(h, k);
        const auto& arr = m_mat4ArrayOverrides.at(k);
        HashCombine(h, arr.size());
        for (const math::Mat4& m : arr) AppendMat4Hash(h, m);
    }

    return h;
}

bool MaterialInstance::IsBatchEquivalent(const MaterialInstance& other) const {
    if (m_base.get() != other.m_base.get()) return false;
    if (m_albedoOverride != other.m_albedoOverride) return false;
    if (m_metallicOverride != other.m_metallicOverride) return false;
    if (m_roughnessOverride != other.m_roughnessOverride) return false;
    if (m_emissionOverride != other.m_emissionOverride) return false;
    if (m_alphaOverride != other.m_alphaOverride) return false;

    if (m_textureOverrides.size() != other.m_textureOverrides.size()) return false;
    for (const auto& kv : m_textureOverrides) {
        auto it = other.m_textureOverrides.find(kv.first);
        if (it == other.m_textureOverrides.end()) return false;
        if (kv.second.get() != it->second.get()) return false;
    }

    if (!CompareScalarMap(m_floatOverrides, other.m_floatOverrides)) return false;
    if (!CompareScalarMap(m_vec3Overrides, other.m_vec3Overrides)) return false;
    if (!CompareScalarMap(m_vec4Overrides, other.m_vec4Overrides)) return false;

    if (m_mat4ArrayOverrides.size() != other.m_mat4ArrayOverrides.size()) return false;
    for (const auto& kv : m_mat4ArrayOverrides) {
        auto it = other.m_mat4ArrayOverrides.find(kv.first);
        if (it == other.m_mat4ArrayOverrides.end()) return false;
        const auto& a = kv.second;
        const auto& b = it->second;
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (!Mat4Equal(a[i], b[i])) return false;
        }
    }
    return true;
}

void MaterialInstance::UpdateForwardResourceSet(rhi::IDevice* device, rhi::IDescriptorSet* set) const {
    if (!device || !set || !m_base) return;
    rhi::IBuffer* buf = GetMaterialBufferRHI(device);
    if (buf) set->UpdateUniformBuffer(0, buf, 0, static_cast<uint32_t>(kMaterialBlockStd140Size));
    if (rhi::ITexture* t = GetTextureRHI("u_AlbedoTexture")) set->UpdateTexture(1, t);
    if (rhi::ITexture* t = GetTextureRHI("u_NormalTexture")) set->UpdateTexture(2, t);
    if (rhi::ITexture* t = GetTextureRHI("u_MetallicRoughnessTexture")) set->UpdateTexture(3, t);
    else if (rhi::ITexture* t = GetTextureRHI("u_MetallicTexture")) set->UpdateTexture(3, t);
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
