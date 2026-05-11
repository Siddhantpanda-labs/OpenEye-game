#include "BuiltinResourceManagerNew.h"

#include "api/core/Log.h"
#include "api/geometry/BuiltinMeshIds.h"
#include "api/rendering/MaterialRecord.h"
#include "api/rendering/MeshData.h"
#include "api/rendering/ResourceCache.h"

#include "eyelib/geometry/EditableMesh.h"
#include "eyelib/geometry/primitives/MeshPrimitives.h"
#include "eyelib/render/MeshDescriptor.h"

#include <atomic>
#include <utility>

namespace openeye::player {

namespace {

/** Build a public `MeshData` from the EditableMesh produced by the geometry primitives.
 *  The legacy `MeshDescriptor` it returns is field-compatible with our public POD. */
eyelib::api::rendering::MeshData PublicMeshDataFromEditable(const eyelib::geometry::EditableMesh& edit) {
    const auto desc = edit.GetDescriptor();
    eyelib::api::rendering::MeshData out{};
    out.vertices.reserve(desc.vertices.size());
    for (const auto& v : desc.vertices) {
        eyelib::api::rendering::Vertex pv{};
        pv.position    = v.position;
        pv.normal      = v.normal;
        pv.uv          = v.uv;
        pv.uv1         = v.uv1;
        pv.tangent     = v.tangent;
        pv.color       = v.color;
        pv.boneIndices = v.boneIndices;
        pv.boneWeights = v.boneWeights;
        out.vertices.push_back(pv);
    }
    out.indices32 = desc.indices32;
    out.indices16 = desc.indices16;
    out.indexFormat = desc.indexFormat == eyelib::render::IndexFormat::UInt16
                          ? eyelib::api::rendering::IndexFormat::UInt16
                          : eyelib::api::rendering::IndexFormat::UInt32;
    out.submeshes.reserve(desc.submeshes.size());
    for (const auto& s : desc.submeshes) {
        eyelib::api::rendering::Submesh ps{};
        ps.indexOffset   = s.indexOffset;
        ps.indexCount    = s.indexCount;
        ps.materialIndex = s.materialIndex;
        out.submeshes.push_back(ps);
    }
    return out;
}

/** Generate the editable mesh for a builtin id; empty when id is not a builtin we ship. */
bool BuildEditablePrimitive(const std::string& meshId, eyelib::geometry::EditableMesh& out) {
    namespace BuiltinMesh = eyelib::api::geometry::BuiltinMesh;
    using eyelib::geometry::MeshPrimitives;
    if (meshId == BuiltinMesh::Cube)     { MeshPrimitives::CreateCube(out, 0.5f);                          return true; }
    if (meshId == BuiltinMesh::Sphere)   { MeshPrimitives::CreateSphere(out, 0.5f, 32, 16);                return true; }
    if (meshId == BuiltinMesh::Plane)    { MeshPrimitives::CreatePlane(out, 1.0f, 1.0f, 1, 1);             return true; }
    if (meshId == BuiltinMesh::Cylinder) { MeshPrimitives::CreateCylinder(out, 0.5f, 1.0f, 32, 1, true, true); return true; }
    if (meshId == BuiltinMesh::Cone)     { MeshPrimitives::CreateCone(out, 0.5f, 1.0f, 32, true);          return true; }
    if (meshId == BuiltinMesh::Torus)    { MeshPrimitives::CreateTorus(out, 0.5f, 0.2f, 32, 16);           return true; }
    return false;
}

/** Stable synthetic AssetId namespace for builtin primitives + default material so the
 *  cache treats them like any other asset (deduped, mutated through the same fan-out). */
eyelib::api::AssetId BuiltinAssetId(const std::string& key) {
    return eyelib::api::AssetId::FromString(std::string("builtin-rendering://") + key);
}

void LogColdPathOnce() {
    static std::atomic<bool> warned{false};
    bool expected = false;
    if (warned.compare_exchange_strong(expected, true)) {
        eyelib::api::Log::Warn(
            "BuiltinResourceManagerNew — ResourceCache cold (no live device); returning "
            "invalid primitive entries (M4.4 flip).");
    }
}

}  // namespace

BuiltinResourceManagerNew::BuiltinResourceManagerNew(eyelib::api::rendering::ResourceCache* cache) noexcept
    : m_cache(cache),
      m_defaultMaterialAssetId(BuiltinAssetId("default-material")) {}

BuiltinResourceManagerNew::~BuiltinResourceManagerNew() = default;

eyelib::api::rendering::Material BuiltinResourceManagerNew::GetDefaultMaterial() {
    if (!m_cache) return {};
    if (m_cache->HasMaterial(m_defaultMaterialAssetId)) {
        return m_cache->GetMaterial(m_defaultMaterialAssetId);
    }
    eyelib::api::rendering::MaterialRecord record{};  // defaults: white PBR, fully rough.
    return m_cache->CreateMaterial(m_defaultMaterialAssetId, record);
}

BuiltinResourceManagerNew::PrimitiveEntry BuiltinResourceManagerNew::GetPrimitiveEntry(const std::string& meshId) {
    using namespace eyelib::api::geometry;
    if (!IsBuiltinMeshId(meshId) || !m_cache) return {};

    auto it = m_primitiveCache.find(meshId);
    if (it != m_primitiveCache.end()) return it->second;

    if (!m_cache->IsValid()) {
        LogColdPathOnce();
        return {};
    }

    eyelib::geometry::EditableMesh edit;
    if (!BuildEditablePrimitive(meshId, edit)) return {};

    auto idIt = m_primitiveAssetIds.find(meshId);
    eyelib::api::AssetId aid;
    if (idIt == m_primitiveAssetIds.end()) {
        aid = BuiltinAssetId(std::string("primitive/") + meshId);
        m_primitiveAssetIds.emplace(meshId, aid);
    } else {
        aid = idIt->second;
    }

    PrimitiveEntry entry;
    entry.mesh     = m_cache->CreateMesh(aid, PublicMeshDataFromEditable(edit));
    entry.material = GetDefaultMaterial();
    if (!entry.mesh.IsValid()) return {};

    m_primitiveCache.emplace(meshId, entry);
    return entry;
}

}  // namespace openeye::player
