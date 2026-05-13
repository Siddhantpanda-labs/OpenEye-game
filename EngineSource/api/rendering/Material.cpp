#include "api/rendering/Material.h"
#include "api/rendering/MaterialInstance.h"

#include "api/rendering/MaterialRecord.h"
#include "api/rendering/ResourceCache.h"
#include "api/rendering/Texture2D.h"
#include "api/rendering/internal/ResourceAccess.h"
#include "eyelib/rendering/materials/MaterialInstance.h"
#include "eyelib/rendering/resources/Texture2D.h"

#include <utility>

namespace eyelib {
namespace api {
namespace rendering {

// =====================================================================================
// Material
// =====================================================================================
//
// The Material PIMPL is a thin handle into a `ResourceCache` entry. Setters mutate the
// cache-owned `MaterialRecord` in place via the `ResourceCacheAccess::MutateMaterial`
// seam; the cache then re-publishes the updated row to every attached pipeline's
// `RenderResources::MaterialTable`.
//
// The cache pointer is borrowed (the editor / player owns the cache and keeps it alive
// for the duration of any handle that references it). A handle whose cache has been
// destroyed degrades to `IsValid() == false`; setters become silent no-ops.

class Material::Impl {
public:
    ResourceCache*       cache           = nullptr;  // borrowed; cache owns the record
    eyelib::api::AssetId assetId         = eyelib::api::AssetId::Invalid();
    uint32_t             materialIdToken = 0u;       // 0 = invalid (default row)

    // Snapshot the live record for getter calls. Returns the default record (all-default
    // factors) when the handle is invalid so getters never crash. Defined here so
    // helpers can reach the private Impl without an extra friend declaration.
    static MaterialRecord ReadRecord(const Impl* impl) {
        MaterialRecord copy{};
        if (!impl || !impl->cache) return copy;
        ResourceCacheAccess::MutateMaterial(
            *impl->cache, impl->assetId,
            [&](MaterialRecord& live) { copy = live; });
        return copy;
    }

    // Texture setter helper: writes `texture`'s cache token into the matching record slot.
    // Passing an invalid (or non-cache-vended) texture clears the slot to
    // `kInvalidTextureHandle`. Resolution back to a sampler binding lights up when the
    // new module's texture binding-table feature lands; until then this only stores the
    // contract in the MaterialRecord row.
    static void SetTextureSlot(Impl* impl, const Texture2D& texture,
                               uint32_t MaterialRecord::*slot) {
        if (!impl || !impl->cache) return;
        const uint32_t token = Texture2DAccess::GetCacheToken(texture);
        const uint32_t value = (token != 0u) ? token : kInvalidTextureHandle;
        ResourceCacheAccess::MutateMaterial(
            *impl->cache, impl->assetId,
            [&](MaterialRecord& r) { r.*slot = value; });
    }
};

Material::Material() noexcept : m_impl(std::make_unique<Impl>()) {}
Material::~Material() = default;

Material::Material(const Material& other) : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
Material& Material::operator=(const Material& other) {
    if (this != &other) *m_impl = *other.m_impl;
    return *this;
}
Material::Material(Material&& other) noexcept = default;
Material& Material::operator=(Material&& other) noexcept = default;

bool Material::IsValid() const noexcept {
    return m_impl && m_impl->cache != nullptr && m_impl->materialIdToken != 0u;
}

uint32_t Material::GetMaterialId() const noexcept {
    return (m_impl && m_impl->cache) ? m_impl->materialIdToken : 0u;
}

void Material::SetAlbedo(const math::Vec3& color) {
    if (!IsValid()) return;
    ResourceCacheAccess::MutateMaterial(
        *m_impl->cache, m_impl->assetId,
        [&](MaterialRecord& r) {
            r.baseColorFactor.x = color.x;
            r.baseColorFactor.y = color.y;
            r.baseColorFactor.z = color.z;
        });
}

void Material::SetMetallic(float value) {
    if (!IsValid()) return;
    ResourceCacheAccess::MutateMaterial(
        *m_impl->cache, m_impl->assetId,
        [&](MaterialRecord& r) { r.metallicFactor = value; });
}

void Material::SetRoughness(float value) {
    if (!IsValid()) return;
    ResourceCacheAccess::MutateMaterial(
        *m_impl->cache, m_impl->assetId,
        [&](MaterialRecord& r) { r.roughnessFactor = value; });
}

void Material::SetEmission(const math::Vec3& color) {
    if (!IsValid()) return;
    ResourceCacheAccess::MutateMaterial(
        *m_impl->cache, m_impl->assetId,
        [&](MaterialRecord& r) {
            r.emissiveFactor.x = color.x;
            r.emissiveFactor.y = color.y;
            r.emissiveFactor.z = color.z;
        });
}

void Material::SetAO(float value) {
    if (!IsValid()) return;
    ResourceCacheAccess::MutateMaterial(
        *m_impl->cache, m_impl->assetId,
        [&](MaterialRecord& r) { r.occlusionStrength = value; });
}

void Material::SetAlpha(float value) {
    if (!IsValid()) return;
    ResourceCacheAccess::MutateMaterial(
        *m_impl->cache, m_impl->assetId,
        [&](MaterialRecord& r) { r.baseColorFactor.w = value; });
}

void Material::SetUVTransform(const math::Vec4& transform) {
    if (!IsValid()) return;
    ResourceCacheAccess::MutateMaterial(
        *m_impl->cache, m_impl->assetId,
        [&](MaterialRecord& r) { r.uvTransform = transform; });
}

void Material::SetUVRotation(float rotation) {
    if (!IsValid()) return;
    ResourceCacheAccess::MutateMaterial(
        *m_impl->cache, m_impl->assetId,
        [&](MaterialRecord& r) { r.uvRotation = rotation; });
}

void Material::SetTexCoordIndex(float index) {
    if (!IsValid()) return;
    ResourceCacheAccess::MutateMaterial(
        *m_impl->cache, m_impl->assetId,
        [&](MaterialRecord& r) { r.texCoordIndex = index; });
}

void Material::SetFlipV(float flip) {
    if (!IsValid()) return;
    ResourceCacheAccess::MutateMaterial(
        *m_impl->cache, m_impl->assetId,
        [&](MaterialRecord& r) { r.flipV = flip; });
}

math::Vec3 Material::GetAlbedo() const {
    const MaterialRecord r = Impl::ReadRecord(m_impl.get());
    return {r.baseColorFactor.x, r.baseColorFactor.y, r.baseColorFactor.z};
}

float Material::GetMetallic() const  { return Impl::ReadRecord(m_impl.get()).metallicFactor; }
float Material::GetRoughness() const { return Impl::ReadRecord(m_impl.get()).roughnessFactor; }

math::Vec3 Material::GetEmission() const {
    const MaterialRecord r = Impl::ReadRecord(m_impl.get());
    return {r.emissiveFactor.x, r.emissiveFactor.y, r.emissiveFactor.z};
}

float Material::GetAO() const    { return Impl::ReadRecord(m_impl.get()).occlusionStrength; }
float Material::GetAlpha() const { return Impl::ReadRecord(m_impl.get()).baseColorFactor.w; }

math::Vec4 Material::GetUVTransform() const { return Impl::ReadRecord(m_impl.get()).uvTransform; }
float Material::GetUVRotation() const       { return Impl::ReadRecord(m_impl.get()).uvRotation; }
float Material::GetTexCoordIndex() const    { return Impl::ReadRecord(m_impl.get()).texCoordIndex; }
float Material::GetFlipV() const            { return Impl::ReadRecord(m_impl.get()).flipV; }

void Material::SetAlbedoTexture(const Texture2D& texture)            { Impl::SetTextureSlot(m_impl.get(), texture, &MaterialRecord::baseColorTexture); }
void Material::SetNormalTexture(const Texture2D& texture)            { Impl::SetTextureSlot(m_impl.get(), texture, &MaterialRecord::normalTexture); }
void Material::SetMetallicTexture(const Texture2D& texture)          { Impl::SetTextureSlot(m_impl.get(), texture, &MaterialRecord::metallicTexture); }
void Material::SetRoughnessTexture(const Texture2D& texture)         { Impl::SetTextureSlot(m_impl.get(), texture, &MaterialRecord::roughnessTexture); }
void Material::SetMetallicRoughnessTexture(const Texture2D& texture) { Impl::SetTextureSlot(m_impl.get(), texture, &MaterialRecord::metallicRoughnessTexture); }
void Material::SetAOTexture(const Texture2D& texture)                { Impl::SetTextureSlot(m_impl.get(), texture, &MaterialRecord::occlusionTexture); }
void Material::SetEmissiveTexture(const Texture2D& texture)          { Impl::SetTextureSlot(m_impl.get(), texture, &MaterialRecord::emissiveTexture); }

Material MaterialAccess::Wrap(ResourceCache* cache,
                              const eyelib::api::AssetId& assetId,
                              uint32_t materialIdToken) {
    Material handle{};
    handle.m_impl->cache           = cache;
    handle.m_impl->assetId         = assetId;
    handle.m_impl->materialIdToken = materialIdToken;
    return handle;
}

// =====================================================================================
// MaterialInstance
// =====================================================================================

class MaterialInstance::Impl {
public:
    std::shared_ptr<eyelib::rendering::MaterialInstance> instance;
    uint32_t materialIdToken = 0u;
};

MaterialInstance::MaterialInstance() noexcept : m_impl(std::make_unique<Impl>()) {}
MaterialInstance::~MaterialInstance() = default;

MaterialInstance::MaterialInstance(const MaterialInstance& other) : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
MaterialInstance& MaterialInstance::operator=(const MaterialInstance& other) {
    if (this != &other) *m_impl = *other.m_impl;
    return *this;
}
MaterialInstance::MaterialInstance(MaterialInstance&& other) noexcept = default;
MaterialInstance& MaterialInstance::operator=(MaterialInstance&& other) noexcept = default;

bool MaterialInstance::IsValid() const noexcept {
    return m_impl && m_impl->instance != nullptr;
}

uint32_t MaterialInstance::GetMaterialId() const noexcept {
    return IsValid() ? m_impl->materialIdToken : 0u;
}

Material MaterialInstance::GetBaseMaterial() const {
    // The new MaterialAccess::Wrap signature requires a `(ResourceCache*, AssetId)`
    // pair to bind the returned handle to its cache-owned `MaterialRecord`. The
    // internal `eyelib::rendering::MaterialInstance` carries neither, so we cannot
    // synthesize a fully-valid handle here. Returning an invalid handle is safe — the
    // public `Material` getters / setters all guard on `IsValid()`. A future redesign
    // can plumb the cache pointer through the instance once `ResourceCache` vends
    // `MaterialInstance` handles too (no current caller).
    return Material{};
}

void MaterialInstance::SetAlbedoOverride(const math::Vec3& color)   { if (IsValid()) m_impl->instance->SetAlbedoOverride(color); }
void MaterialInstance::SetMetallicOverride(float value)             { if (IsValid()) m_impl->instance->SetMetallicOverride(value); }
void MaterialInstance::SetRoughnessOverride(float value)            { if (IsValid()) m_impl->instance->SetRoughnessOverride(value); }
void MaterialInstance::SetEmissionOverride(const math::Vec3& color) { if (IsValid()) m_impl->instance->SetEmissionOverride(color); }
void MaterialInstance::SetAlphaOverride(float value)                { if (IsValid()) m_impl->instance->SetAlphaOverride(value); }

void MaterialInstance::SetTextureOverride(const char* slotName, const Texture2D& texture) {
    if (!IsValid() || !slotName) return;
    m_impl->instance->SetTextureOverride(std::string{slotName}, Texture2DAccess::Unwrap(texture));
}

void MaterialInstance::ClearOverrides() { if (IsValid()) m_impl->instance->ClearOverrides(); }

math::Vec3 MaterialInstance::GetAlbedo() const   { return IsValid() ? m_impl->instance->GetAlbedo()   : math::Vec3{0.8f}; }
float      MaterialInstance::GetMetallic() const { return IsValid() ? m_impl->instance->GetMetallic() : 0.0f; }
float      MaterialInstance::GetRoughness() const{ return IsValid() ? m_impl->instance->GetRoughness(): 0.5f; }
math::Vec3 MaterialInstance::GetEmission() const { return IsValid() ? m_impl->instance->GetEmission() : math::Vec3{0.0f}; }
float      MaterialInstance::GetAlpha() const    { return IsValid() ? m_impl->instance->GetAlpha()    : 1.0f; }

MaterialInstance MaterialInstanceAccess::Wrap(std::shared_ptr<eyelib::rendering::MaterialInstance> internalInstance,
                                              uint32_t materialIdToken) {
    MaterialInstance handle{};
    handle.m_impl->instance = std::move(internalInstance);
    handle.m_impl->materialIdToken = materialIdToken;
    return handle;
}

} // namespace rendering
} // namespace api
} // namespace eyelib
