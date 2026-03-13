#include "BuiltinResourceManager.h"

#include "api/core/All.h"
#include "api/geometry/Primitives.h"

#include <cassert>

namespace openeye::player {

void BuiltinResourceManager::InitializeDefaultResources() {
    if (!m_device) return;
    EnsureDefaultMaterial();
    EnsureSkinnedMaterial();
}

void BuiltinResourceManager::EnsureDefaultMaterial() {
    if (m_defaultMaterial) return;
    auto shader = std::make_shared<eyelib::api::Shader>();
    if (m_device && shader->Load(m_device, "passes/forward/forward")) {
        auto mat = std::make_shared<eyelib::api::Material>();
        mat->SetShader(shader);
        mat->SetAlbedo(eyelib::math::Vec3(1.0f, 1.0f, 1.0f));
        m_defaultShader = shader;
        m_defaultMaterial = mat;
        return;
    }
    if (!m_device) {
        eyelib::api::core::Log::Warn("BuiltinResourceManager: default shader not loaded — device is null (passes/forward/forward).");
    } else {
        eyelib::api::core::Log::Warn("BuiltinResourceManager: default shader failed to load (passes/forward/forward). Using empty material.");
    }
#ifndef NDEBUG
    assert(false && "BuiltinResourceManager: default shader required for primitives failed. Set device before InitializeDefaultResources; ensure passes/forward/forward exists.");
#endif
    m_defaultMaterial = std::make_shared<eyelib::api::Material>();
}

void BuiltinResourceManager::EnsureSkinnedMaterial() {
    if (m_skinnedShader) return;
    auto shader = std::make_shared<eyelib::api::Shader>();
    if (m_device && shader->Load(m_device, "passes/forward/forward_skinned")) {
        m_skinnedShader = shader;
        return;
    }
    if (!m_device) {
        eyelib::api::core::Log::Warn("BuiltinResourceManager: skinned shader not loaded — device is null (passes/forward/forward_skinned).");
    } else {
        eyelib::api::core::Log::Warn("BuiltinResourceManager: skinned shader failed to load (passes/forward/forward_skinned).");
    }
#ifndef NDEBUG
    assert(false && "BuiltinResourceManager: skinned shader failed. Set device before InitializeDefaultResources; ensure passes/forward/forward_skinned exists.");
#endif
}

std::shared_ptr<eyelib::api::Material> BuiltinResourceManager::GetDefaultMaterial() {
    EnsureDefaultMaterial();
    return m_defaultMaterial;
}

std::shared_ptr<eyelib::api::Shader> BuiltinResourceManager::GetSkinnedShader() {
    EnsureSkinnedMaterial();
    return m_skinnedShader;
}

BuiltinResourceManager::PrimitiveEntry BuiltinResourceManager::GetPrimitiveEntry(const std::string& meshId) {
    using namespace eyelib::api::geometry;
    if (!IsBuiltinMeshId(meshId)) return {};
    auto it = m_primitiveCache.find(meshId);
    if (it != m_primitiveCache.end()) return it->second;

    PrimitiveEntry entry{};
    std::shared_ptr<eyelib::api::Mesh> mesh;
    if (meshId == BuiltinMesh::Cube) {
        mesh = Primitives::CreateCube(0.5f);
    } else if (meshId == BuiltinMesh::Sphere) {
        mesh = Primitives::CreateSphere(0.5f, 32, 16);
    } else if (meshId == BuiltinMesh::Plane) {
        mesh = Primitives::CreatePlane(1.0f, 1.0f, 1, 1);
    } else if (meshId == BuiltinMesh::Cylinder) {
        mesh = Primitives::CreateCylinder(0.5f, 1.0f, 32, 1, true, true);
    } else if (meshId == BuiltinMesh::Cone) {
        mesh = Primitives::CreateCone(0.5f, 1.0f, 32, true);
    } else if (meshId == BuiltinMesh::Torus) {
        mesh = Primitives::CreateTorus(0.5f, 0.2f, 32, 16);
    }
    if (mesh) {
        entry.mesh = std::move(mesh);
        entry.material = GetDefaultMaterial();
    }
    m_primitiveCache[meshId] = entry;
    return entry;
}

} // namespace openeye::player
