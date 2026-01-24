#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/AABB.h"
#include "eyelib/math/Sphere.h"
#include "eyelib/math/Capsule.h"
#include "eyelib/math/Frustum.h"
#include "eyelib/math/Ray.h"
#include "eyelib/math/Plane.h"
#include <optional>
#include <cmath>

namespace eyelib {
namespace math {

struct RayHit {
    bool hit = false;
    float t = 0.0f;
    Vec3 point{};
    Vec3 normal{};
};

/** Ray vs plane. Returns hit point if intersection (t >= 0). */
inline std::optional<Vec3> RayPlane(const Ray& ray, const Plane& plane) {
    float nd = glm::dot(ray.direction, plane.normal);
    if (std::fabs(nd) < 1e-6f)
        return std::nullopt;
    float t = -(glm::dot(ray.origin, plane.normal) + plane.distance) / nd;
    if (t < 0.0f)
        return std::nullopt;
    return ray.GetPoint(t);
}

/** Ray vs sphere. */
inline RayHit RaySphere(const Ray& ray, const Sphere& s) {
    RayHit out;
    Vec3 oc = ray.origin - s.center;
    float a = glm::dot(ray.direction, ray.direction);
    float b = 2.0f * glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - s.radius * s.radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f)
        return out;
    float t = (-b - std::sqrt(disc)) / (2.0f * a);
    if (t < 0.0f)
        t = (-b + std::sqrt(disc)) / (2.0f * a);
    if (t < 0.0f)
        return out;
    out.hit = true;
    out.t = t;
    out.point = ray.GetPoint(t);
    out.normal = glm::normalize(out.point - s.center);
    return out;
}

/** Ray vs AABB. */
inline RayHit RayAABB(const Ray& ray, const AABB& aabb) {
    RayHit out;
    const float eps = 1e-6f;
    auto safeInv = [eps](float x) { return std::fabs(x) < eps ? (x >= 0 ? 1e6f : -1e6f) : 1.0f / x; };
    Vec3 invD(safeInv(ray.direction.x), safeInv(ray.direction.y), safeInv(ray.direction.z));
    Vec3 t0 = (aabb.min - ray.origin) * invD;
    Vec3 t1 = (aabb.max - ray.origin) * invD;
    Vec3 tmin = glm::min(t0, t1);
    Vec3 tmax = glm::max(t0, t1);
    float tNear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
    float tFar = glm::min(glm::min(tmax.x, tmax.y), tmax.z);
    if (tNear > tFar || tFar < 0.0f)
        return out;
    float t = tNear >= 0.0f ? tNear : tFar;
    out.hit = true;
    out.t = t;
    out.point = ray.GetPoint(t);
    Vec3 c = aabb.GetCenter();
    Vec3 e = aabb.GetExtents();
    Vec3 d = out.point - c;
    const float faceEps = 1e-5f;
    if (std::fabs(d.x - e.x) < faceEps) out.normal = Vec3(1, 0, 0);
    else if (std::fabs(d.x + e.x) < faceEps) out.normal = Vec3(-1, 0, 0);
    else if (std::fabs(d.y - e.y) < faceEps) out.normal = Vec3(0, 1, 0);
    else if (std::fabs(d.y + e.y) < faceEps) out.normal = Vec3(0, -1, 0);
    else if (std::fabs(d.z - e.z) < faceEps) out.normal = Vec3(0, 0, 1);
    else out.normal = Vec3(0, 0, -1);
    if (glm::dot(out.normal, ray.direction) > 0.0f)
        out.normal = -out.normal;
    return out;
}

/** Ray vs triangle (Möller–Trumbore). */
inline RayHit RayTriangle(const Ray& ray, const Vec3& v0, const Vec3& v1, const Vec3& v2) {
    RayHit out;
    Vec3 e1 = v1 - v0;
    Vec3 e2 = v2 - v0;
    Vec3 h = glm::cross(ray.direction, e2);
    float a = glm::dot(e1, h);
    if (std::fabs(a) < 1e-6f)
        return out;
    float f = 1.0f / a;
    Vec3 s = ray.origin - v0;
    float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f)
        return out;
    Vec3 q = glm::cross(s, e1);
    float v = f * glm::dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f)
        return out;
    float t = f * glm::dot(e2, q);
    if (t < 0.0f)
        return out;
    out.hit = true;
    out.t = t;
    out.point = ray.GetPoint(t);
    out.normal = glm::normalize(glm::cross(e1, e2));
    return out;
}

inline bool SphereSphere(const Sphere& a, const Sphere& b) {
    return a.Intersects(b);
}

inline bool AABBAABB(const AABB& a, const AABB& b) {
    return a.Intersects(b);
}

inline bool FrustumSphere(const Frustum& f, const Sphere& s) {
    return f.Contains(s);
}

/** 0 = outside, 1 = intersect, 2 = inside. */
inline int FrustumAABB(const Frustum& f, const AABB& aabb) {
    return f.Contains(aabb);
}

} // namespace math
} // namespace eyelib
