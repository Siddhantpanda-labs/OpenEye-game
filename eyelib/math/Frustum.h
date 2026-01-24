#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/AABB.h"
#include "eyelib/math/Sphere.h"
#include <array>
#include <cmath>

namespace eyelib {
namespace math {

/** View frustum: 6 planes (ax+by+cz+d=0). */
class Frustum {
public:
    enum class Plane : int {
        Left = 0,
        Right = 1,
        Bottom = 2,
        Top = 3,
        Near = 4,
        Far = 5
    };

    Frustum() = default;

    /** Extract frustum planes from combined view-projection matrix. */
    void FromMatrix(const Mat4& viewProj) {
        // Left
        m_planes[0].x = viewProj[0][3] + viewProj[0][0];
        m_planes[0].y = viewProj[1][3] + viewProj[1][0];
        m_planes[0].z = viewProj[2][3] + viewProj[2][0];
        m_planes[0].w = viewProj[3][3] + viewProj[3][0];
        // Right
        m_planes[1].x = viewProj[0][3] - viewProj[0][0];
        m_planes[1].y = viewProj[1][3] - viewProj[1][0];
        m_planes[1].z = viewProj[2][3] - viewProj[2][0];
        m_planes[1].w = viewProj[3][3] - viewProj[3][0];
        // Bottom
        m_planes[2].x = viewProj[0][3] + viewProj[0][1];
        m_planes[2].y = viewProj[1][3] + viewProj[1][1];
        m_planes[2].z = viewProj[2][3] + viewProj[2][1];
        m_planes[2].w = viewProj[3][3] + viewProj[3][1];
        // Top
        m_planes[3].x = viewProj[0][3] - viewProj[0][1];
        m_planes[3].y = viewProj[1][3] - viewProj[1][1];
        m_planes[3].z = viewProj[2][3] - viewProj[2][1];
        m_planes[3].w = viewProj[3][3] - viewProj[3][1];
        // Near
        m_planes[4].x = viewProj[0][3] + viewProj[0][2];
        m_planes[4].y = viewProj[1][3] + viewProj[1][2];
        m_planes[4].z = viewProj[2][3] + viewProj[2][2];
        m_planes[4].w = viewProj[3][3] + viewProj[3][2];
        // Far
        m_planes[5].x = viewProj[0][3] - viewProj[0][2];
        m_planes[5].y = viewProj[1][3] - viewProj[1][2];
        m_planes[5].z = viewProj[2][3] - viewProj[2][2];
        m_planes[5].w = viewProj[3][3] - viewProj[3][2];
        for (Vec4& p : m_planes)
            NormalizePlane(p);
    }

    const Vec4& GetPlane(Plane plane) const {
        return m_planes[static_cast<int>(plane)];
    }

    bool Contains(const Vec3& point) const {
        for (const Vec4& p : m_planes) {
            if (glm::dot(Vec3(p), point) + p.w < 0.0f)
                return false;
        }
        return true;
    }

    /** 0 = outside, 1 = intersect, 2 = inside. */
    int Contains(const AABB& aabb) const {
        int result = 2;
        for (const Vec4& plane : m_planes) {
            Vec3 n(plane);
            Vec3 positive(plane.x > 0.0f ? aabb.max.x : aabb.min.x,
                          plane.y > 0.0f ? aabb.max.y : aabb.min.y,
                          plane.z > 0.0f ? aabb.max.z : aabb.min.z);
            if (glm::dot(n, positive) + plane.w < 0.0f)
                return 0;
            Vec3 negative(plane.x < 0.0f ? aabb.max.x : aabb.min.x,
                          plane.y < 0.0f ? aabb.max.y : aabb.min.y,
                          plane.z < 0.0f ? aabb.max.z : aabb.min.z);
            if (glm::dot(n, negative) + plane.w < 0.0f)
                result = 1;
        }
        return result;
    }

    bool Contains(const Sphere& s) const {
        for (const Vec4& p : m_planes) {
            if (glm::dot(Vec3(p), s.center) + p.w < -s.radius)
                return false;
        }
        return true;
    }

    /** 8 corner points from inverse view-projection (NDC cube [-1,1]^3). */
    std::array<Vec3, 8> GetCorners(const Mat4& inverseViewProj) const {
        std::array<Vec3, 8> ndc = {
            Vec3(-1, -1, -1), Vec3( 1, -1, -1), Vec3( 1,  1, -1), Vec3(-1,  1, -1),
            Vec3(-1, -1,  1), Vec3( 1, -1,  1), Vec3( 1,  1,  1), Vec3(-1,  1,  1)
        };
        std::array<Vec3, 8> out;
        for (size_t i = 0; i < 8; ++i) {
            Vec4 h = inverseViewProj * Vec4(ndc[i], 1.0f);
            out[i] = Vec3(h) / h.w;
        }
        return out;
    }

private:
    std::array<Vec4, 6> m_planes{};

    static void NormalizePlane(Vec4& p) {
        float l = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        if (l > 1e-6f) {
            p.x /= l;
            p.y /= l;
            p.z /= l;
            p.w /= l;
        }
    }
};

} // namespace math
} // namespace eyelib
