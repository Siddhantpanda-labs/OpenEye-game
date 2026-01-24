/** Phase 1 Math verification: compile all math headers and exercise API. */
#include "eyelib/math/All.h"
#include <cmath>

namespace eyelib {
namespace math {

static void VerifyPhase1Math() {
    // Types
    Vec3 a(1, 0, 0), b(0, 1, 0);
    (void)a;
    (void)b;

    // AABB
    AABB box(Vec3(0), Vec3(1, 1, 1));
    (void)box.GetCenter();
    (void)box.GetExtents();
    (void)box.IsValid();
    (void)box.Contains(Vec3(0.5f));
    (void)box.Intersects(box);
    (void)box.Union(box);

    // Sphere
    Sphere s(Vec3(0), 1.0f);
    (void)s.Contains(Vec3(0, 0, 0));
    (void)s.Distance(Vec3(2, 0, 0));

    // Capsule
    Capsule cap(Vec3(0), Vec3(0, 1, 0), 0.5f);
    (void)cap.GetCenter();
    (void)cap.GetHeight();

    // Frustum
    Frustum fr;
    Mat4 proj = Perspective(3.14159265f / 4.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    Mat4 view = LookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    fr.FromMatrix(proj * view);
    (void)fr.Contains(Vec3(0, 0, 0));

    // Ray & Plane
    Ray r(Vec3(0), Vec3(0, 0, -1));
    (void)r.GetPoint(1.0f);
    Plane pl = Plane::FromNormalPoint(Vec3(0, 1, 0), Vec3(0, 0, 0));

    // Intersection
    (void)RayPlane(r, pl);
    (void)RaySphere(r, s);
    (void)RayAABB(r, box);
    (void)RayTriangle(r, Vec3(0), Vec3(1, 0, 0), Vec3(0, 1, 0));
    (void)SphereSphere(s, s);
    (void)AABBAABB(box, box);
    (void)FrustumSphere(fr, s);
    (void)FrustumAABB(fr, box);

    // TransformUtils
    Mat4 M = Compose(Vec3(0), Quat(1, 0, 0, 0), Vec3(1));
    Vec3 pos;
    Quat rot;
    Vec3 scl;
    Decompose(M, pos, rot, scl);
    (void)Inverse(M);
    (void)NormalMatrix(M);

    // Interpolation
    (void)Lerp(0.0f, 1.0f, 0.5f);
    (void)Slerp(Quat(1, 0, 0, 0), Quat(1, 0, 0, 0), 0.5f);
    (void)Bezier(Vec3(0), Vec3(1, 0, 0), Vec3(1, 1, 0), Vec3(0, 1, 0), 0.5f);
    (void)EaseIn(0.5f, 2.0f);
}

} // namespace math
} // namespace eyelib
