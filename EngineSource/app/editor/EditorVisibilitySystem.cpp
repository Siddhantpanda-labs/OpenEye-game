#include "EditorVisibilitySystem.h"

#include <cmath>

namespace openeye::editor {

namespace {

inline void NormalizePlane(EditorVisibilitySystem::Plane& p) {
    const float len = std::sqrt(p.a * p.a + p.b * p.b + p.c * p.c);
    if (len <= 1e-6f) return;
    p.a /= len; p.b /= len; p.c /= len; p.d /= len;
}

inline float PlaneDistance(const EditorVisibilitySystem::Plane& p, const eyelib::math::Vec3& v) {
    return p.a * v.x + p.b * v.y + p.c * v.z + p.d;
}

} // namespace

void EditorVisibilitySystem::BeginFrame(const eyelib::api::world::World::RenderSnapshot& snapshot) {
    m_candidates.clear();
    m_visibleEntities.clear();

    m_candidates.reserve(snapshot.renderables.size() + snapshot.meshRenderers.size());

    std::unordered_set<EntityId, EntityIdHash> seen;
    seen.reserve(snapshot.renderables.size() + snapshot.meshRenderers.size());

    const auto addCandidate = [&](EntityId entity, bool visible) {
        if (!visible) return;
        if (!seen.emplace(entity).second) return;
        const auto t = snapshot.GetTransform(entity);
        if (!t) return;
        Candidate c{};
        c.entity = entity;
        c.position = t->position;
        c.radius = 1.0f; // conservative default until per-mesh bounds are wired.
        m_candidates.push_back(c);
    };

    for (const auto& e : snapshot.renderables)
        addCandidate(e.entity, e.component.visible);
    for (const auto& e : snapshot.meshRenderers)
        addCandidate(e.entity, e.component.visible && !e.component.meshAssetId.empty());
}

void EditorVisibilitySystem::BuildFrustumPlanes(const eyelib::math::Mat4& vp, Plane out[6]) {
    // Extract rows from column-major matrix.
    const float r00 = vp[0][0], r01 = vp[1][0], r02 = vp[2][0], r03 = vp[3][0];
    const float r10 = vp[0][1], r11 = vp[1][1], r12 = vp[2][1], r13 = vp[3][1];
    const float r20 = vp[0][2], r21 = vp[1][2], r22 = vp[2][2], r23 = vp[3][2];
    const float r30 = vp[0][3], r31 = vp[1][3], r32 = vp[2][3], r33 = vp[3][3];

    // Left, Right, Bottom, Top, Near, Far
    out[0] = { r03 + r00, r13 + r10, r23 + r20, r33 + r30 };
    out[1] = { r03 - r00, r13 - r10, r23 - r20, r33 - r30 };
    out[2] = { r03 + r01, r13 + r11, r23 + r21, r33 + r31 };
    out[3] = { r03 - r01, r13 - r11, r23 - r21, r33 - r31 };
    out[4] = { r03 + r02, r13 + r12, r23 + r22, r33 + r32 };
    out[5] = { r03 - r02, r13 - r12, r23 - r22, r33 - r32 };

    for (int i = 0; i < 6; ++i) NormalizePlane(out[i]);
}

bool EditorVisibilitySystem::SphereIntersectsFrustum(const Plane planes[6], const eyelib::math::Vec3& center, float radius) {
    for (int i = 0; i < 6; ++i) {
        if (PlaneDistance(planes[i], center) < -radius)
            return false;
    }
    return true;
}

void EditorVisibilitySystem::BuildCameraVisibility(const eyelib::api::rendering::CameraView& camera) {
    m_visibleEntities.clear();
    Plane planes[6];
    const auto vp = camera.projection * camera.view;
    BuildFrustumPlanes(vp, planes);

    m_visibleEntities.reserve(m_candidates.size());
    for (const auto& c : m_candidates) {
        if (SphereIntersectsFrustum(planes, c.position, c.radius))
            m_visibleEntities.emplace(c.entity);
    }
}

bool EditorVisibilitySystem::IsVisible(EntityId entity) const {
    if (m_visibleEntities.empty())
        return true;
    return m_visibleEntities.find(entity) != m_visibleEntities.end();
}

} // namespace openeye::editor

