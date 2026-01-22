#include "XPBDClothSystem.h"
#include "../Components.h"
#include "core/time/Time.h"
#include "core/log/Log.h"
#include <glm/glm.hpp>
#include <algorithm>

namespace OpenEye {
namespace ECS {

static inline void ApplyDistanceConstraint(
    glm::vec3& x1, float w1,
    glm::vec3& x2, float w2,
    float restLen,
    float compliance,
    float dt
) {
    const float wsum = w1 + w2;
    if (wsum <= 0.0f) return;

    glm::vec3 d = x1 - x2;
    const float len = glm::length(d);
    if (len < 1e-6f) return;

    const glm::vec3 n = d / len;
    const float C = len - restLen;

    // XPBD: alpha = compliance / dt^2
    const float alpha = (dt > 0.0f) ? (compliance / (dt * dt)) : 0.0f;
    const float dl = -C / (wsum + alpha);

    x1 += n * (dl * w1);
    x2 -= n * (dl * w2);
}

void XPBDClothSystem::Update(Scene& scene, float deltaTime) {
    auto& registry = scene.GetRegistry();

    auto view = registry.view<XPBDClothComponent, Renderable, Transform>();
    for (auto e : view) {
        auto& cloth = view.get<XPBDClothComponent>(e);
        auto& renderable = view.get<Renderable>(e);
        const auto& transform = view.get<Transform>(e);

        if (!cloth.enabled || !renderable.mesh) continue;

        const float dt = std::min(std::max(deltaTime, 0.0f), 1.0f / 20.0f); // clamp to 50ms
        if (dt <= 0.0f) continue;

        const int w = cloth.widthSegments + 1;
        const int h = cloth.heightSegments + 1;
        const int count = w * h;
        if ((int)cloth.positions.size() != count || (int)cloth.prevPositions.size() != count || (int)cloth.invMass.size() != count) {
            continue;
        }

        // External accel
        glm::vec3 accel = cloth.gravity;
        if (cloth.windEnabled && cloth.windStrength > 0.0f) {
            glm::vec3 wd = cloth.windDirection;
            float len = glm::length(wd);
            if (len > 1e-4f) wd /= len;
            else wd = glm::vec3(1, 0, 0);
            accel += wd * cloth.windStrength;
        }

        // Verlet integrate
        const float damping = glm::clamp(cloth.damping, 0.0f, 1.0f);
        const float dt2 = dt * dt;

        for (int i = 0; i < count; ++i) {
            if (cloth.invMass[i] <= 0.0f) continue; // pinned

            glm::vec3 x = cloth.positions[i];
            glm::vec3 v = (cloth.positions[i] - cloth.prevPositions[i]) * (1.0f - damping);
            cloth.prevPositions[i] = x;
            cloth.positions[i] = x + v + accel * dt2;
        }

        // Solve constraints
        const float compliance = std::max(cloth.compliance, 0.0f);
        for (int it = 0; it < cloth.iterations; ++it) {
            // structural constraints (horizontal + vertical)
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    const int idx = y * w + x;
                    if (x + 1 < w) {
                        const int idxR = idx + 1;
                        ApplyDistanceConstraint(cloth.positions[idx], cloth.invMass[idx],
                                                cloth.positions[idxR], cloth.invMass[idxR],
                                                cloth.restLenX, compliance, dt);
                    }
                    if (y + 1 < h) {
                        const int idxD = idx + w;
                        ApplyDistanceConstraint(cloth.positions[idx], cloth.invMass[idx],
                                                cloth.positions[idxD], cloth.invMass[idxD],
                                                cloth.restLenY, compliance, dt);
                    }
                }
            }
            // shear constraints (diagonals)
            if (cloth.enableShear) {
                const float restShear = std::sqrt(cloth.restLenX * cloth.restLenX + cloth.restLenY * cloth.restLenY);
                for (int y = 0; y + 1 < h; ++y) {
                    for (int x = 0; x + 1 < w; ++x) {
                        const int idx = y * w + x;
                        const int idxDR = idx + w + 1;
                        const int idxDL = idx + w;
                        const int idxUR = idx + 1;
                        ApplyDistanceConstraint(cloth.positions[idx], cloth.invMass[idx],
                                                cloth.positions[idxDR], cloth.invMass[idxDR],
                                                restShear, compliance, dt);
                        ApplyDistanceConstraint(cloth.positions[idxUR], cloth.invMass[idxUR],
                                                cloth.positions[idxDL], cloth.invMass[idxDL],
                                                restShear, compliance, dt);
                    }
                }
            }
        }

        // Write back to render mesh vertices (local space)
        auto& verts = renderable.mesh->GetVertices();
        if ((int)verts.size() != count) {
            continue;
        }

        for (int i = 0; i < count; ++i) {
            verts[i].position = cloth.positions[i];
            verts[i].normal = glm::vec3(0, 0, 1); // temp, recalculated below
        }

        // Recompute normals from triangles
        for (int i = 0; i < count; ++i) verts[i].normal = glm::vec3(0.0f);
        for (int y = 0; y + 1 < h; ++y) {
            for (int x = 0; x + 1 < w; ++x) {
                int i0 = y * w + x;
                int i1 = y * w + (x + 1);
                int i2 = (y + 1) * w + x;
                int i3 = (y + 1) * w + (x + 1);

                glm::vec3 p0 = verts[i0].position;
                glm::vec3 p1 = verts[i1].position;
                glm::vec3 p2 = verts[i2].position;
                glm::vec3 p3 = verts[i3].position;

                glm::vec3 n0 = glm::cross(p1 - p0, p2 - p0);
                glm::vec3 n1 = glm::cross(p3 - p1, p2 - p1);

                verts[i0].normal += n0;
                verts[i1].normal += n0 + n1;
                verts[i2].normal += n0 + n1;
                verts[i3].normal += n1;
            }
        }
        for (int i = 0; i < count; ++i) {
            float len = glm::length(verts[i].normal);
            if (len > 1e-6f) verts[i].normal /= len;
            else verts[i].normal = glm::vec3(0, 0, 1);
        }

        renderable.mesh->MarkDirty();
    }
}

} // namespace ECS
} // namespace OpenEye

