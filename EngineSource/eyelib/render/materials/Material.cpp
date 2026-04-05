#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialShader.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/materials/MaterialDescriptor.h"
#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/render/resources/PassResourceBuilders.h"
#include <functional>
#include <cstring>

namespace eyelib {
namespace render {

Material::Material() = default;

void Material::SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture) {
    m_textures[name] = std::move(texture);
    m_hashDirty = true;
}

void Material::RemoveTexture(const std::string& name) {
    if (m_textures.erase(name) != 0)
        m_hashDirty = true;
}

std::shared_ptr<Texture2D> Material::GetTexture(const std::string& name) const {
    auto it = m_textures.find(name);
    return it != m_textures.end() ? it->second : nullptr;
}

rhi::ITexture* Material::GetTextureRHI(const std::string& name) const {
    std::shared_ptr<Texture2D> tex = GetTexture(name);
    return (tex && tex->IsValid()) ? tex->GetTextureRHI() : nullptr;
}

bool Material::BindTextureToSlot(const std::string& name, int unit) const {
    std::shared_ptr<Texture2D> tex = GetTexture(name);
    if (!tex || !tex->IsValid()) return false;
    tex->Bind(unit);
    return true;
}

void Material::SetAlbedo(const math::Vec3& color) { m_albedo = color; m_hashDirty = true; }
void Material::SetMetallic(float v) { m_metallic = v; m_hashDirty = true; }
void Material::SetRoughness(float v) { m_roughness = v; m_hashDirty = true; }
void Material::SetEmission(const math::Vec3& color) { m_emission = color; m_hashDirty = true; }
void Material::SetAO(float v) { m_ao = v; m_hashDirty = true; }
void Material::SetAlpha(float v) { m_alpha = v; m_hashDirty = true; }

void Material::SetFloat(const std::string& name, float value) {
    m_floats[name] = value;
    m_hashDirty = true;
}

void Material::SetVec3(const std::string& name, const math::Vec3& value) {
    m_vec3s[name] = value;
    m_hashDirty = true;
}

void Material::SetVec4(const std::string& name, const math::Vec4& value) {
    m_vec4s[name] = value;
    m_hashDirty = true;
}

void Material::SetMat4Array(const std::string& name, const std::vector<math::Mat4>& values) {
    m_mat4Arrays[name] = values;
    m_hashDirty = true;
}

float Material::GetFloat(const std::string& name, float defaultValue) const {
    auto it = m_floats.find(name);
    return it != m_floats.end() ? it->second : defaultValue;
}

math::Vec3 Material::GetVec3(const std::string& name, const math::Vec3& defaultValue) const {
    auto it = m_vec3s.find(name);
    return it != m_vec3s.end() ? it->second : defaultValue;
}

math::Vec4 Material::GetVec4(const std::string& name, const math::Vec4& defaultValue) const {
    auto it = m_vec4s.find(name);
    return it != m_vec4s.end() ? it->second : defaultValue;
}

void Material::SetKeyword(const std::string& keyword, bool enabled) {
    m_keywords[keyword] = enabled;
    m_hashDirty = true;
}

bool Material::IsKeywordEnabled(const std::string& keyword) const {
    auto it = m_keywords.find(keyword);
    return it != m_keywords.end() && it->second;
}

void Material::ApplyToShader(rhi::IDevice* device, Shader* shader, rhi::ICommandList* cmd) const {
    if (!device || !shader) return;

    if (cmd) {
        rhi::IBuffer* buf = GetMaterialBufferRHI(device);
        if (buf)
            cmd->BindUniformBuffer(buf, rhi::UniformBinding::PerMaterial, 0, static_cast<uint32_t>(kMaterialBlockStd140Size));
    }

    int unit = 0;
    for (const auto& p : m_textures) {
        if (p.second && p.second->IsValid()) {
            p.second->Bind(unit);
            shader->SetInt(p.first, unit);
            ++unit;
        }
    }

    // PBR params (albedo, metallic, roughness, ao, emission, alpha, reflective) come from MaterialBlock UBO (binding 2); pipeline binds it before draw.

    for (const auto& p : m_floats) shader->SetFloat(p.first, p.second);
    for (const auto& p : m_vec3s) shader->SetVec3(p.first, p.second);
    for (const auto& p : m_vec4s) shader->SetVec4(p.first, p.second);
    for (const auto& p : m_mat4Arrays) {
        if (!p.second.empty())
            shader->SetMat4Array(p.first, p.second.data(), p.second.size());
    }
}

void Material::UpdateForwardResourceSet(rhi::IDevice* device, rhi::IDescriptorSet* set) const {
    resources::UpdateMaterialDescriptorSetWithDefaults(device, set, this, nullptr);
}

size_t Material::GetHash() const {
    if (m_hashDirty) {
        UpdateHash();
        m_hashDirty = false;
    }
    return m_cachedHash;
}

void Material::UpdateHash() const {
    std::hash<std::string> sh;
    std::hash<float> fh;
    std::hash<void*> ph;

    size_t h = 0;
    h ^= (size_t)m_shaderIndex + 0x9e3779b9 + (h << 6) + (h >> 2);
    for (const auto& p : m_textures) {
        h ^= sh(p.first) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= ph(p.second.get()) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    h ^= fh(m_albedo.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_albedo.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_albedo.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_metallic) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_roughness) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_ao) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_alpha) + 0x9e3779b9 + (h << 6) + (h >> 2);

    m_cachedHash = h;
}

void Material::ForEachTexture(std::function<void(const std::string&, const std::shared_ptr<Texture2D>&)> fn) const {
    for (const auto& p : m_textures) fn(p.first, p.second);
}

void Material::ForEachFloat(std::function<void(const std::string&, float)> fn) const {
    for (const auto& p : m_floats) fn(p.first, p.second);
}

void Material::ForEachVec3(std::function<void(const std::string&, const math::Vec3&)> fn) const {
    for (const auto& p : m_vec3s) fn(p.first, p.second);
}

void Material::ForEachVec4(std::function<void(const std::string&, const math::Vec4&)> fn) const {
    for (const auto& p : m_vec4s) fn(p.first, p.second);
}

void Material::ForEachMat4Array(std::function<void(const std::string&, const math::Mat4*, size_t)> fn) const {
    for (const auto& p : m_mat4Arrays) {
        if (!p.second.empty())
            fn(p.first, p.second.data(), p.second.size());
    }
}

namespace {
/** std140 layout: vec3 albedo (12) + pad (4), 4 floats (16), vec3 emission (12) + reflective (4) = 48 bytes. */
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
static_assert(sizeof(MaterialBlockStd140) == kMaterialBlockStd140Size, "MaterialBlock std140 size");
} // namespace

void Material::EnsureMaterialBuffer(rhi::IDevice* device) const {
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
    data.albedo[0] = m_albedo.x;
    data.albedo[1] = m_albedo.y;
    data.albedo[2] = m_albedo.z;
    data.metallic = m_metallic;
    data.roughness = m_roughness;
    data.ao = m_ao;
    data.alpha = m_alpha;
    data.emission[0] = m_emission.x;
    data.emission[1] = m_emission.y;
    data.emission[2] = m_emission.z;
    data.reflective = m_reflectiveStrength;
    data.pad0 = 0.0f;
    m_materialBuffer->Update(&data, 0, kMaterialBlockStd140Size);
}

rhi::IBuffer* Material::GetMaterialBufferRHI(rhi::IDevice* device) const {
    EnsureMaterialBuffer(device);
    return m_materialBuffer.get();
}

static void FillMaterialFromDescriptor(
    Material& mat,
    const MaterialDescriptor& desc,
    const std::function<std::shared_ptr<Shader>(const std::string& name)>& resolveShader,
    const std::function<std::shared_ptr<Texture2D>(const std::string& path)>& resolveTexture) {
    (void)resolveShader;
    if (!desc.shaderAssetPath.empty())
        mat.SetMaterialShaderId(MaterialShaderIds::PBR);
    else if (desc.materialShaderId != kDefaultMaterialTypeId)
        mat.SetMaterialShaderId(desc.materialShaderId);
    for (const auto& p : desc.texturePaths) {
        if (p.second.empty())
            mat.RemoveTexture(p.first);
        else {
            auto tex = resolveTexture(p.second);
            if (tex)
                mat.SetTexture(p.first, tex);
        }
    }
    for (const auto& p : desc.floats) mat.SetFloat(p.first, p.second);
    for (const auto& p : desc.vec3s) mat.SetVec3(p.first, p.second);
    for (const auto& p : desc.vec4s) mat.SetVec4(p.first, p.second);
    for (const auto& p : desc.mat4Arrays) mat.SetMat4Array(p.first, p.second);
    for (const auto& p : desc.keywords) mat.SetKeyword(p.first, p.second);
    mat.SetAlbedo(desc.albedo);
    mat.SetMetallic(desc.metallic);
    mat.SetRoughness(desc.roughness);
    mat.SetEmission(desc.emission);
    mat.SetAO(desc.ao);
    mat.SetAlpha(desc.alpha);
    mat.SetShadingModel(desc.shadingModel);
    mat.SetBlending(desc.blending);
    mat.SetTwoSided(desc.twoSided);
    mat.SetRefraction(desc.refraction);
    mat.SetReflectiveStrength(desc.reflectiveStrength);
    mat.SetFragmentSnippet(desc.fragmentSnippet);
    mat.SetShadingSnippet(desc.shadingSnippet);
}

std::shared_ptr<Material> CreateMaterialFromDescriptor(
    const MaterialDescriptor& desc,
    std::function<std::shared_ptr<Shader>(const std::string& name)> resolveShader,
    std::function<std::shared_ptr<Texture2D>(const std::string& path)> resolveTexture) {
    auto mat = std::make_shared<Material>();
    FillMaterialFromDescriptor(*mat, desc, resolveShader, resolveTexture);
    return mat;
}

void ApplyMaterialFromDescriptor(
    Material& mat,
    const MaterialDescriptor& desc,
    std::function<std::shared_ptr<Shader>(const std::string& name)> resolveShader,
    std::function<std::shared_ptr<Texture2D>(const std::string& path)> resolveTexture) {
    FillMaterialFromDescriptor(mat, desc, resolveShader, resolveTexture);
}

int ComputeForwardAlbedoTextureFlag(const Material* mat, const MaterialInstance* mi) {
    auto hasAlbedo = [&](const char* name) -> bool {
        if (mi) return mi->GetTextureRHI(name) != nullptr;
        return mat && mat->GetTextureRHI(name) != nullptr;
    };
    return hasAlbedo("u_AlbedoTexture") ? 1 : 0;
}

} // namespace render
} // namespace eyelib
