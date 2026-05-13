#include "RendererGPUResourceCacheNew.h"

#include "api/core/Log.h"
#include "api/rendering/MaterialRecord.h"
#include "api/rendering/MeshData.h"
#include "api/rendering/ResourceCache.h"
#include "api/rendering/TextureData.h"

// Translation only — these legacy headers are read-only descriptor sources from the
// asset registry; the new resources they produce live entirely in `api/rendering`.
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/TextureDescriptor.h"
#include "eyelib/render/materials/MaterialDescriptor.h"

#include <atomic>

namespace openeye::player {

namespace {

eyelib::api::rendering::IndexFormat ToPublicIndexFormat(eyelib::render::IndexFormat f) noexcept {
    return f == eyelib::render::IndexFormat::UInt16
               ? eyelib::api::rendering::IndexFormat::UInt16
               : eyelib::api::rendering::IndexFormat::UInt32;
}

eyelib::api::rendering::Vertex ToPublicVertex(const eyelib::render::Vertex& v) noexcept {
    eyelib::api::rendering::Vertex out{};
    out.position    = v.position;
    out.normal      = v.normal;
    out.uv          = v.uv;
    out.uv1         = v.uv1;
    out.tangent     = v.tangent;
    out.color       = v.color;
    out.boneIndices = v.boneIndices;
    out.boneWeights = v.boneWeights;
    return out;
}

eyelib::api::rendering::Submesh ToPublicSubmesh(const eyelib::render::Submesh& s) noexcept {
    eyelib::api::rendering::Submesh out{};
    out.indexOffset   = s.indexOffset;
    out.indexCount    = s.indexCount;
    out.materialIndex = s.materialIndex;
    return out;
}

eyelib::api::rendering::MeshData ToPublicMeshData(const eyelib::render::MeshDescriptor& src) {
    eyelib::api::rendering::MeshData out{};
    out.vertices.reserve(src.vertices.size());
    for (const auto& v : src.vertices) out.vertices.push_back(ToPublicVertex(v));
    out.indices32   = src.indices32;
    out.indices16   = src.indices16;
    out.indexFormat = ToPublicIndexFormat(src.indexFormat);
    out.submeshes.reserve(src.submeshes.size());
    for (const auto& s : src.submeshes) out.submeshes.push_back(ToPublicSubmesh(s));
    return out;
}

eyelib::api::rendering::TextureData ToPublicTextureData(const eyelib::render::TextureDescriptor& src) {
    eyelib::api::rendering::TextureData out{};
    out.filePath        = src.filePath;
    out.isSRGB          = src.isSRGB;
    out.generateMipmaps = src.generateMipmaps;
    return out;
}

/**
 * Translate a legacy `MaterialDescriptor` to the public `MaterialRecord`. Only the PBR
 * scalar / vector fields and alpha-mode are crossable today; the new module's
 * `MaterialRecord` does not carry the legacy authoring extras (shader keywords, custom
 * snippets, scattered float / vec3 / vec4 maps), so those are silently dropped here —
 * the new render module's deferred lighting MaterialTable does not consume them.
 *
 * Texture slot tokens stay `kInvalidTextureHandle` for now: the new API expects
 * cache-assigned tokens (returned by `ResourceCache::CreateTexture`) rather than raw
 * asset ids. Wiring the lookup chain (legacy texture-asset-guid → AssetId → cache token)
 * is folded in below as best-effort using the borrowed registry / cache.
 */
eyelib::api::rendering::MaterialRecord ToPublicMaterialRecord(const eyelib::render::MaterialDescriptor& src,
                                                              eyelib::api::AssetRegistry* registry,
                                                              eyelib::api::rendering::ResourceCache* cache,
                                                              RendererGPUResourceCacheNew& self) {
    eyelib::api::rendering::MaterialRecord out{};
    out.baseColorFactor = eyelib::math::Vec4(src.albedo.x, src.albedo.y, src.albedo.z, src.alpha);
    out.emissiveFactor  = eyelib::math::Vec4(src.emission.x, src.emission.y, src.emission.z, 1.0f);
    out.metallicFactor  = src.metallic;
    out.roughnessFactor = src.roughness;
    out.normalScale     = 1.0f;
    out.occlusionStrength = src.ao;
    out.alphaCutoff     = 0.5f;
    switch (src.blending) {
        case eyelib::render::Blending::Opaque:
            out.alphaMode = eyelib::api::rendering::MaterialAlphaMode::Opaque;
            break;
        case eyelib::render::Blending::Translucent:
        case eyelib::render::Blending::Additive:
        case eyelib::render::Blending::Modulate:
            out.alphaMode = eyelib::api::rendering::MaterialAlphaMode::Blend;
            break;
    }
    out.doubleSided = src.twoSided;

    // Read canonical properties from the legacy descriptor maps
    auto itVec4 = src.vec4s.find("u_UVTransform");
    if (itVec4 != src.vec4s.end()) out.uvTransform = itVec4->second;

    auto itRot = src.floats.find("u_UVRotation");
    if (itRot != src.floats.end()) out.uvRotation = itRot->second;

    auto itTexIdx = src.floats.find("u_TexCoordIndex");
    if (itTexIdx != src.floats.end()) out.texCoordIndex = itTexIdx->second;

    auto itFlip = src.floats.find("u_FlipV");
    if (itFlip != src.floats.end()) out.flipV = itFlip->second;

    // Note: Texture slot tokens default to `kInvalidTextureHandle` in the record; 
    // they are populated right after handle creation in `GetMaterial` via the public
    // `Material::Set*Texture` setter API to ensure the cache tokens map correctly.
    return out;
}

void LogColdPathOnce(const char* what) {
    static std::atomic<bool> warned{false};
    bool expected = false;
    if (warned.compare_exchange_strong(expected, true)) {
        std::string msg = "RendererGPUResourceCacheNew::";
        msg += what;
        msg += " — ResourceCache cold (no live device); returning invalid handle (M4.4 flip).";
        eyelib::api::Log::Warn(msg);
    }
}

}  // namespace

RendererGPUResourceCacheNew::RendererGPUResourceCacheNew(eyelib::api::rendering::ResourceCache* cache,
                                                         eyelib::api::AssetRegistry* registry) noexcept
    : m_cache(cache), m_registry(registry) {}

RendererGPUResourceCacheNew::~RendererGPUResourceCacheNew() = default;

eyelib::api::rendering::Mesh RendererGPUResourceCacheNew::GetMesh(const eyelib::api::AssetId& id) {
    if (!id.IsValid() || !m_cache) return {};
    if (m_cache->HasMesh(id)) return m_cache->GetMesh(id);
    if (!m_cache->IsValid()) {
        LogColdPathOnce("GetMesh");
        return {};
    }
    if (!m_registry) return {};
    const auto* desc = m_registry->GetMeshDescriptor(id);
    if (!desc) return {};
    auto handle = m_cache->CreateMesh(id, ToPublicMeshData(*desc));
    if (handle.IsValid()) m_meshesResolved.insert(id);
    return handle;
}

eyelib::api::rendering::Texture2D RendererGPUResourceCacheNew::GetTexture(const eyelib::api::AssetId& id) {
    if (!id.IsValid() || !m_cache) return {};
    if (m_cache->HasTexture(id)) return m_cache->GetTexture(id);
    if (!m_cache->IsValid()) {
        LogColdPathOnce("GetTexture");
        return {};
    }
    if (!m_registry) return {};
    const auto* desc = m_registry->GetTextureDescriptor(id);
    if (!desc || desc->filePath.empty()) return {};
    auto handle = m_cache->CreateTexture(id, ToPublicTextureData(*desc));
    if (handle.IsValid()) m_texturesResolved.insert(id);
    return handle;
}

eyelib::api::rendering::Material RendererGPUResourceCacheNew::GetMaterial(const eyelib::api::AssetId& id) {
    if (!id.IsValid() || !m_cache) return {};
    if (m_cache->HasMaterial(id)) return m_cache->GetMaterial(id);
    // Material rows can be authored without a live device (cache stores the CPU record);
    // do not gate on `IsValid()` here.
    if (!m_registry) return {};
    const auto* desc = m_registry->GetMaterialDescriptor(id);
    if (!desc) return {};

    // 1. Author initial CPU record (scalar PBR properties)
    auto record = ToPublicMaterialRecord(*desc, m_registry, m_cache, *this);
    auto handle = m_cache->CreateMaterial(id, record);

    // 2. Bind canonical texture assets to update backend MaterialTable binding tokens
    if (handle.IsValid()) {
        auto getTex = [&](const std::string& key) -> eyelib::api::rendering::Texture2D {
            auto it = desc->texturePaths.find(key);
            if (it == desc->texturePaths.end() || it->second.empty()) return {};
            eyelib::api::AssetId tid = eyelib::api::AssetId::FromString(it->second);
            return tid.IsValid() ? this->GetTexture(tid) : eyelib::api::rendering::Texture2D{};
        };

        auto albedo = getTex("u_AlbedoTexture");
        auto normal = getTex("u_NormalTexture");
        auto mr     = getTex("u_MetallicRoughnessTexture");
        auto emissive = getTex("u_EmissiveTexture");
        auto ao     = getTex("u_AOTexture");
        auto metallic = getTex("u_MetallicTexture");
        auto roughness = getTex("u_RoughnessTexture");

        if (albedo.IsValid())   handle.SetAlbedoTexture(albedo);
        if (normal.IsValid())   handle.SetNormalTexture(normal);
        if (mr.IsValid())       handle.SetMetallicRoughnessTexture(mr);
        if (emissive.IsValid()) handle.SetEmissiveTexture(emissive);
        if (ao.IsValid())       handle.SetAOTexture(ao);
        if (metallic.IsValid()) handle.SetMetallicTexture(metallic);
        if (roughness.IsValid()) handle.SetRoughnessTexture(roughness);

        m_materialsResolved.insert(id);
    }

    return handle;
}

void RendererGPUResourceCacheNew::OnDeviceLost() {
    m_meshesResolved.clear();
    m_texturesResolved.clear();
    // Material rows survive device loss inside the cache (CPU-only); keep the resolution
    // memo so we do not redundantly re-author them after the device comes back.
}

void RendererGPUResourceCacheNew::InvalidateMaterial(const eyelib::api::AssetId& id) {
    if (!id.IsValid()) return;
    m_materialsResolved.erase(id);
    if (m_cache) m_cache->Remove(id);
}

}  // namespace openeye::player
