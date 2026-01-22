#include "JellyBlobSystem.h"
#include "../Components.h"
#include "core/log/Log.h"
#include <glm/glm.hpp>
#include <algorithm>

// Optional: tune Bullet point-to-point constraints if present
#include <btBulletDynamicsCommon.h>

namespace OpenEye {
namespace ECS {

static inline float B0(float t) { const float a = 1.0f - t; return a * a; }
static inline float B1(float t) { return 2.0f * t * (1.0f - t); }
static inline float B2(float t) { return t * t; }

static inline glm::vec3 EvalFFD3(
    const glm::vec3& uvw,
    const glm::vec3 P[3][3][3]
) {
    const float u = glm::clamp(uvw.x, 0.0f, 1.0f);
    const float v = glm::clamp(uvw.y, 0.0f, 1.0f);
    const float w = glm::clamp(uvw.z, 0.0f, 1.0f);

    const float Bu[3] = { B0(u), B1(u), B2(u) };
    const float Bv[3] = { B0(v), B1(v), B2(v) };
    const float Bw[3] = { B0(w), B1(w), B2(w) };

    glm::vec3 out(0.0f);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                out += P[i][j][k] * (Bu[i] * Bv[j] * Bw[k]);
            }
        }
    }
    return out;
}

void JellyBlobSystem::Update(Scene& scene, float /*deltaTime*/) {
    auto& registry = scene.GetRegistry();
    auto view = registry.view<JellyBlobComponent, Renderable, Transform>();

    for (auto blobEntity : view) {
        auto& blob = view.get<JellyBlobComponent>(blobEntity);
        auto& renderable = view.get<Renderable>(blobEntity);
        const auto& blobTransform = view.get<Transform>(blobEntity);

        if (!blob.enabled || !renderable.mesh) continue;
        if (blob.dim != 3) continue;
        if ((int)blob.nodeEntities.size() != 27) continue;

        // Cache base vertices once (rest shape)
        if (blob.baseVertices.empty()) {
            blob.baseVertices = renderable.mesh->GetVertices();
        }
        if (blob.baseVertices.empty()) continue;

        const glm::vec3 restMin = blob.restMin;
        const glm::vec3 restMax = blob.restMax;
        const glm::vec3 restExt = restMax - restMin;
        if (restExt.x <= 1e-6f || restExt.y <= 1e-6f || restExt.z <= 1e-6f) continue;

        // Build 3x3x3 control points from node entity transforms (blob-local space).
        glm::vec3 P[3][3][3];
        for (int z = 0; z < 3; ++z) {
            for (int y = 0; y < 3; ++y) {
                for (int x = 0; x < 3; ++x) {
                    const int idx = (z * 9) + (y * 3) + x;
                    const Entity nodeE = blob.nodeEntities[size_t(idx)];
                    if (!registry.valid(nodeE) || !registry.all_of<Transform>(nodeE)) {
                        P[x][y][z] = glm::vec3(0.0f);
                        continue;
                    }
                    const auto& nt = registry.get<Transform>(nodeE);
                    P[x][y][z] = nt.position - blobTransform.position; // world -> blob-local

                    // Debug visibility toggle for node spheres
                    if (registry.all_of<Renderable>(nodeE)) {
                        registry.get<Renderable>(nodeE).visible = blob.showDebugNodes;
                    }
                }
            }
        }

        // Update constraints parameters if present (non-owning pointers).
        const float tau = glm::clamp(blob.springTau, 0.0f, 1.0f);
        const float damp = glm::clamp(blob.springDamping, 0.0f, 1.0f);
        for (auto* c : blob.constraints) {
            if (!c) continue;
            // Avoid dynamic_cast (RTTI may be disabled in this build -> "no RTTI data" abort)
            if (c->getConstraintType() != POINT2POINT_CONSTRAINT_TYPE) continue;
            auto* p2p = static_cast<btPoint2PointConstraint*>(c);
            p2p->m_setting.m_tau = tau;
            p2p->m_setting.m_damping = damp;
        }

        // Deform mesh vertices using FFD mapping (u,v,w) computed from rest cage.
        auto& verts = renderable.mesh->GetVertices();
        if (verts.size() != blob.baseVertices.size()) continue;

        for (size_t i = 0; i < verts.size(); ++i) {
            const glm::vec3 p0 = blob.baseVertices[i].position;
            const glm::vec3 uvw(
                (p0.x - restMin.x) / restExt.x,
                (p0.y - restMin.y) / restExt.y,
                (p0.z - restMin.z) / restExt.z
            );
            verts[i].position = EvalFFD3(uvw, P);
            verts[i].normal = glm::vec3(0.0f); // recompute below
        }

        // Recompute normals from triangles for correct lighting.
        const auto& idx = renderable.mesh->GetIndices();
        for (size_t i = 0; i < verts.size(); ++i) verts[i].normal = glm::vec3(0.0f);
        for (size_t t = 0; t + 2 < idx.size(); t += 3) {
            const uint32_t i0 = idx[t + 0];
            const uint32_t i1 = idx[t + 1];
            const uint32_t i2 = idx[t + 2];
            if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) continue;
            const glm::vec3 p0 = verts[i0].position;
            const glm::vec3 p1 = verts[i1].position;
            const glm::vec3 p2 = verts[i2].position;
            const glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
            verts[i0].normal += n;
            verts[i1].normal += n;
            verts[i2].normal += n;
        }
        for (size_t i = 0; i < verts.size(); ++i) {
            const float len = glm::length(verts[i].normal);
            verts[i].normal = (len > 1e-6f) ? (verts[i].normal / len) : glm::vec3(0.0f, 1.0f, 0.0f);
        }

        renderable.mesh->MarkDirty();
    }
}

} // namespace ECS
} // namespace OpenEye

