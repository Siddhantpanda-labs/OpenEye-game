#include "api/rendering/ResourceCache.h"

#include "api/core/Log.h"
#include "api/rendering/Material.h"
#include "api/rendering/Mesh.h"
#include "api/rendering/RenderPipeline.h"
#include "api/rendering/Texture2D.h"
#include "api/rendering/internal/ResourceAccess.h"

// `ResourceCache.cpp` is the bridge between the public POD descriptors
// (`MeshData`, `TextureData`, `MaterialRecord`) and the new module's resource APIs.
// Per the `api/rendering` charter this TU is allowed to include `eyelib/rendering/...`.
#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/geometry/MeshDescriptor.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/resources/Texture2D.h"
#include "eyelib/rendering/resources/TextureDescriptor.h"
#include "eyelib/rendering/rhi/RHI.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eyelib {
namespace api {
namespace rendering {

namespace {

inline ::eyelib::rendering::rhi::IDevice* AsRhiDevice(RenderDevice device) noexcept {
    return static_cast<::eyelib::rendering::rhi::IDevice*>(device.Get());
}

// Composite key for `AssetId { high, low }`. Combines the two 64-bit halves with the
// xor-shift mix used elsewhere in the engine; collisions are negligible at editor scale
// and would only cost an extra equal-compare in the unordered_map bucket chain.
struct AssetIdHasher {
    size_t operator()(const ::eyelib::api::AssetId& id) const noexcept {
        const uint64_t a = id.high + 0x9e3779b97f4a7c15ull;
        const uint64_t b = id.low  ^ (a << 21) ^ (a >> 11);
        return static_cast<size_t>(a ^ (b * 0xbf58476d1ce4e5b9ull));
    }
};

void LogColdPathOnce(const char* what) {
    static std::atomic<bool> warned{false};
    bool expected = false;
    if (warned.compare_exchange_strong(expected, true)) {
        std::string msg = "api/rendering ResourceCache::";
        msg += what;
        msg += " — cache constructed without a real device; returning invalid handle. "
               "Construct the cache with a live `RenderPipeline::GetDevice()` "
               "(typically at the M3.9 device flip).";
        eyelib::api::Log::Warn(msg);
    }
}

}  // namespace

// =====================================================================================
// Impl
// =====================================================================================

class ResourceCache::Impl {
public:
    explicit Impl(RenderDevice device) noexcept
        : opaqueDevice(device),
          rhiDevice(AsRhiDevice(device)),
          meshResolver(this) {}

    // Device handles. `rhiDevice == nullptr` is the cold-path mode (Create* warn-once
    // and return invalid handles); IsValid() reflects this.
    RenderDevice opaqueDevice;
    ::eyelib::rendering::rhi::IDevice* rhiDevice = nullptr;

    // ----- Storage -------------------------------------------------------------------

    struct MeshEntry {
        std::shared_ptr<::eyelib::rendering::Mesh> internal;
        uint32_t handleToken = 0u;  // 0 = invalid; cache assigns from `nextMeshHandle`.
    };

    struct TextureEntry {
        std::shared_ptr<::eyelib::rendering::Texture2D> internal;
        uint32_t cacheToken = 0u;   // 0 = invalid; cache assigns from `nextTextureToken`.
    };

    struct MaterialEntry {
        MaterialRecord record{};
        uint32_t materialIdToken = 0u;  // 0 reserved for the MaterialTable default row.
    };

    std::unordered_map<::eyelib::api::AssetId, MeshEntry,     AssetIdHasher> meshes;
    std::unordered_map<::eyelib::api::AssetId, TextureEntry,  AssetIdHasher> textures;
    std::unordered_map<::eyelib::api::AssetId, MaterialEntry, AssetIdHasher> materials;

    // Token allocators. Mesh handle 0 is "invalid" by design (matches the M1.5
    // `DrawableInstance::meshHandle` default); material id 0 is the MaterialTable
    // fallback / default row, also not assigned to authored materials. Texture token
    // 0 is reserved as "no cache" (`Texture2DAccess::Wrap` default).
    uint32_t nextMeshHandle    = 1u;
    uint32_t nextMaterialId    = 1u;
    uint32_t nextTextureToken  = 1u;

    // ----- Mesh resolver -------------------------------------------------------------

    // Implements `IMeshHandleResolver` by scanning the mesh map for the matching token.
    // Linear in mesh count per call, but draw lists call this once per draw item; for
    // editor scenes (≤ ~1000 meshes) this is well under the per-frame budget. Promote
    // to a side-table if a profile demands it.
    class CacheMeshResolver final : public ::eyelib::rendering::IMeshHandleResolver {
    public:
        explicit CacheMeshResolver(Impl* o) noexcept : owner(o) {}

        ::eyelib::rendering::MeshHandle TryResolveMesh(uint32_t meshHandle) const noexcept override {
            if (!owner || meshHandle == 0u) return {};
            for (const auto& kv : owner->meshes) {
                if (kv.second.handleToken == meshHandle && kv.second.internal) {
                    return kv.second.internal->GetHandle();
                }
            }
            return {};
        }
    private:
        Impl* owner = nullptr;
    } meshResolver;

    // ----- Pipeline attachment list --------------------------------------------------

    std::vector<RenderPipeline*> attached;

    void RegisterPipeline(RenderPipeline* pipeline) {
        if (!pipeline) return;
        if (std::find(attached.begin(), attached.end(), pipeline) != attached.end()) return;
        attached.push_back(pipeline);
        // Prime the pipeline's MaterialTable with all currently-known records so the
        // pipeline can render scenes that reference them immediately.
        for (const auto& kv : materials) {
            pipeline->SetMaterialRecord(kv.second.materialIdToken, kv.second.record);
        }
    }

    void UnregisterPipeline(RenderPipeline* pipeline) {
        if (!pipeline) return;
        attached.erase(std::remove(attached.begin(), attached.end(), pipeline), attached.end());
    }

    void PublishMaterialRecord(const MaterialEntry& entry) {
        for (auto* p : attached) {
            if (p) p->SetMaterialRecord(entry.materialIdToken, entry.record);
        }
    }
};

// =====================================================================================
// Public API
// =====================================================================================

ResourceCache::ResourceCache(RenderDevice device) noexcept
    : m_impl(std::make_unique<Impl>(device)) {}

ResourceCache::~ResourceCache() {
    if (!m_impl) return;
    // Detach from any still-attached pipelines. Detaching also drops their mesh resolver
    // pointers, which is critical since `meshResolver` lives inside `Impl` and is about
    // to be destroyed.
    auto attached = std::move(m_impl->attached);
    m_impl->attached.clear();
    for (auto* p : attached) {
        if (p) p->DetachResourceCache(*this);
    }
}

bool ResourceCache::IsValid() const noexcept {
    return m_impl && m_impl->rhiDevice != nullptr;
}

// ----- Mesh -------------------------------------------------------------------------

namespace {

::eyelib::rendering::IndexFormat ToInternalIndexFormat(IndexFormat f) noexcept {
    return f == IndexFormat::UInt16
        ? ::eyelib::rendering::IndexFormat::UInt16
        : ::eyelib::rendering::IndexFormat::UInt32;
}

::eyelib::rendering::Vertex ToInternalVertex(const Vertex& v) noexcept {
    ::eyelib::rendering::Vertex out{};
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

::eyelib::rendering::Submesh ToInternalSubmesh(const Submesh& s) noexcept {
    ::eyelib::rendering::Submesh out{};
    out.indexOffset   = s.indexOffset;
    out.indexCount    = s.indexCount;
    out.materialIndex = s.materialIndex;
    return out;
}

::eyelib::rendering::MeshDescriptor ToInternalMeshDescriptor(const MeshData& src) {
    ::eyelib::rendering::MeshDescriptor out{};
    out.vertices.reserve(src.vertices.size());
    for (const auto& v : src.vertices) out.vertices.push_back(ToInternalVertex(v));

    out.indices32 = src.indices32;
    out.indices16 = src.indices16;
    out.indexFormat = ToInternalIndexFormat(src.indexFormat);

    out.submeshes.reserve(src.submeshes.size());
    for (const auto& s : src.submeshes) out.submeshes.push_back(ToInternalSubmesh(s));
    return out;
}

::eyelib::rendering::TextureDescriptor ToInternalTextureDescriptor(const TextureData& src) {
    ::eyelib::rendering::TextureDescriptor out{};
    out.filePath        = src.filePath;
    out.isSRGB          = src.isSRGB;
    out.generateMipmaps = src.generateMipmaps;
    return out;
}

}  // namespace

Mesh ResourceCache::CreateMesh(const ::eyelib::api::AssetId& id, const MeshData& data) {
    if (!m_impl) return Mesh{};
    if (!m_impl->rhiDevice) {
        LogColdPathOnce("CreateMesh");
        return Mesh{};
    }

    auto descriptor = ToInternalMeshDescriptor(data);
    auto internalMesh = ::eyelib::rendering::Mesh::Create(m_impl->rhiDevice, descriptor);
    if (!internalMesh || !internalMesh->IsValid()) {
        eyelib::api::Log::Warn("ResourceCache::CreateMesh: Mesh::Create failed (returning invalid handle).");
        return Mesh{};
    }
    if (id.IsValid()) internalMesh->SetAssetId(id);

    auto& entry = m_impl->meshes[id];
    if (entry.handleToken == 0u) {
        entry.handleToken = m_impl->nextMeshHandle++;
    }
    // Replace internal pointer (upsert): old GPU buffers freed when the previous
    // shared_ptr drops.
    entry.internal = std::move(internalMesh);
    return MeshAccess::Wrap(entry.internal, entry.handleToken);
}

Mesh ResourceCache::GetMesh(const ::eyelib::api::AssetId& id) {
    if (!m_impl) return Mesh{};
    auto it = m_impl->meshes.find(id);
    if (it == m_impl->meshes.end() || !it->second.internal) return Mesh{};
    return MeshAccess::Wrap(it->second.internal, it->second.handleToken);
}

bool ResourceCache::HasMesh(const ::eyelib::api::AssetId& id) const {
    return m_impl && m_impl->meshes.find(id) != m_impl->meshes.end();
}

// ----- Texture ---------------------------------------------------------------------

Texture2D ResourceCache::CreateTexture(const ::eyelib::api::AssetId& id, const TextureData& data) {
    if (!m_impl) return Texture2D{};
    if (!m_impl->rhiDevice) {
        LogColdPathOnce("CreateTexture");
        return Texture2D{};
    }

    auto internalTex = std::make_shared<::eyelib::rendering::Texture2D>();
    auto descriptor  = ToInternalTextureDescriptor(data);
    if (!internalTex->LoadFromDescriptor(m_impl->rhiDevice, descriptor)) {
        eyelib::api::Log::Warn("ResourceCache::CreateTexture: Texture2D::LoadFromDescriptor failed (returning invalid handle).");
        return Texture2D{};
    }
    if (id.IsValid()) internalTex->SetAssetId(id);

    auto& entry = m_impl->textures[id];
    if (entry.cacheToken == 0u) {
        entry.cacheToken = m_impl->nextTextureToken++;
    }
    entry.internal = std::move(internalTex);
    return Texture2DAccess::Wrap(entry.internal, entry.cacheToken);
}

Texture2D ResourceCache::GetTexture(const ::eyelib::api::AssetId& id) {
    if (!m_impl) return Texture2D{};
    auto it = m_impl->textures.find(id);
    if (it == m_impl->textures.end() || !it->second.internal) return Texture2D{};
    return Texture2DAccess::Wrap(it->second.internal, it->second.cacheToken);
}

bool ResourceCache::HasTexture(const ::eyelib::api::AssetId& id) const {
    return m_impl && m_impl->textures.find(id) != m_impl->textures.end();
}

// ----- Material --------------------------------------------------------------------

Material ResourceCache::CreateMaterial(const ::eyelib::api::AssetId& id, const MaterialRecord& record) {
    if (!m_impl) return Material{};

    // Materials are useful even without a device (the record can be authored before the
    // first pipeline attaches), so `CreateMaterial` does NOT require `rhiDevice != nullptr`.
    auto& entry = m_impl->materials[id];
    if (entry.materialIdToken == 0u) {
        entry.materialIdToken = m_impl->nextMaterialId++;
    }
    entry.record = record;
    m_impl->PublishMaterialRecord(entry);
    return MaterialAccess::Wrap(this, id, entry.materialIdToken);
}

Material ResourceCache::GetMaterial(const ::eyelib::api::AssetId& id) {
    if (!m_impl) return Material{};
    auto it = m_impl->materials.find(id);
    if (it == m_impl->materials.end()) return Material{};
    return MaterialAccess::Wrap(this, id, it->second.materialIdToken);
}

bool ResourceCache::HasMaterial(const ::eyelib::api::AssetId& id) const {
    return m_impl && m_impl->materials.find(id) != m_impl->materials.end();
}

// ----- Lifecycle -------------------------------------------------------------------

void ResourceCache::Remove(const ::eyelib::api::AssetId& id) {
    if (!m_impl) return;
    m_impl->meshes.erase(id);
    m_impl->textures.erase(id);
    m_impl->materials.erase(id);
    // Material slots in attached MaterialTables stay allocated (the table never shrinks);
    // the next CreateMaterial(id, ...) re-binds the same slot. This matches the legacy
    // EditorGPUResourceCache behaviour and avoids fragmenting the SSBO.
}

void ResourceCache::OnDeviceLost() {
    if (!m_impl) return;
    m_impl->meshes.clear();
    m_impl->textures.clear();
    // Material records stay (they are device-independent CPU rows). Pipelines will re-upload
    // them on the next frame after the device is recreated.
}

// =====================================================================================
// Friend-only access seam (definitions live here so they can touch `Impl` directly)
// =====================================================================================

void ResourceCacheAccess::RegisterPipeline(ResourceCache& cache, RenderPipeline* pipeline) {
    if (cache.m_impl) cache.m_impl->RegisterPipeline(pipeline);
}

void ResourceCacheAccess::UnregisterPipeline(ResourceCache& cache, RenderPipeline* pipeline) {
    if (cache.m_impl) cache.m_impl->UnregisterPipeline(pipeline);
}

::eyelib::rendering::IMeshHandleResolver* ResourceCacheAccess::GetMeshResolver(ResourceCache& cache) {
    return cache.m_impl ? &cache.m_impl->meshResolver : nullptr;
}

void ResourceCacheAccess::MutateMaterial(ResourceCache& cache,
                                         const ::eyelib::api::AssetId& assetId,
                                         const std::function<void(MaterialRecord&)>& mutator) {
    if (!cache.m_impl) return;
    auto it = cache.m_impl->materials.find(assetId);
    if (it == cache.m_impl->materials.end()) return;
    if (mutator) mutator(it->second.record);
    cache.m_impl->PublishMaterialRecord(it->second);
}

}  // namespace rendering
}  // namespace api
}  // namespace eyelib
