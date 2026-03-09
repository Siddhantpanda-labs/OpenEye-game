#include "DebugDraw.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace eyelib {
namespace debug {

namespace {
std::mutex g_mutex;
std::vector<DebugLine> g_lines;
std::vector<DebugText> g_texts;

static void AddLineInternal(const DebugLine& l) {
    std::scoped_lock lock(g_mutex);
    g_lines.push_back(l);
}

static void AddTextInternal(const DebugText& t) {
    std::scoped_lock lock(g_mutex);
    g_texts.push_back(t);
}

static void BuildBasis(const math::Vec3& normal, math::Vec3& tangent, math::Vec3& bitangent) {
    const math::Vec3 n = glm::normalize(normal);
    const math::Vec3 up = (std::abs(n.y) < 0.99f) ? math::Vec3(0, 1, 0) : math::Vec3(1, 0, 0);
    tangent = glm::normalize(glm::cross(up, n));
    bitangent = glm::normalize(glm::cross(n, tangent));
}

static void AddCircle(const math::Vec3& center, const math::Vec3& normal, float radius,
                      const math::Vec4& color, float durationSeconds, int segments) {
    if (radius <= 0.0f) return;
    segments = std::max(3, segments);
    math::Vec3 tangent, bitangent;
    BuildBasis(normal, tangent, bitangent);
    const float step = 2.0f * 3.1415926535f / static_cast<float>(segments);
    math::Vec3 prev = center + tangent * radius;
    for (int i = 1; i <= segments; ++i) {
        const float t = step * static_cast<float>(i);
        const math::Vec3 p = center + (tangent * std::cos(t) + bitangent * std::sin(t)) * radius;
        DebugDraw::AddLine(prev, p, color, durationSeconds);
        prev = p;
    }
}

static math::Vec3 IntersectPlanes(const math::Vec4& p1, const math::Vec4& p2, const math::Vec4& p3) {
    const math::Vec3 n1(p1), n2(p2), n3(p3);
    const math::Vec3 n2xn3 = glm::cross(n2, n3);
    const math::Vec3 n3xn1 = glm::cross(n3, n1);
    const math::Vec3 n1xn2 = glm::cross(n1, n2);
    const float denom = glm::dot(n1, n2xn3);
    if (std::abs(denom) < 1e-6f) return math::Vec3(0.0f);
    const math::Vec3 numer = (-p1.w * n2xn3) - (p2.w * n3xn1) - (p3.w * n1xn2);
    return numer / denom;
}
} // namespace

void DebugDraw::AddLine(const math::Vec3& a, const math::Vec3& b, const math::Vec4& color, float durationSeconds) {
    DebugLine l;
    l.a = a;
    l.b = b;
    l.color = color;
    l.duration = std::max(0.0f, durationSeconds);
    AddLineInternal(l);
}

void DebugDraw::AddAABB(const math::Vec3& min, const math::Vec3& max, const math::Vec4& color, float durationSeconds) {
    const math::Vec3 p000(min.x, min.y, min.z);
    const math::Vec3 p001(min.x, min.y, max.z);
    const math::Vec3 p010(min.x, max.y, min.z);
    const math::Vec3 p011(min.x, max.y, max.z);
    const math::Vec3 p100(max.x, min.y, min.z);
    const math::Vec3 p101(max.x, min.y, max.z);
    const math::Vec3 p110(max.x, max.y, min.z);
    const math::Vec3 p111(max.x, max.y, max.z);

    AddLine(p000, p001, color, durationSeconds);
    AddLine(p001, p011, color, durationSeconds);
    AddLine(p011, p010, color, durationSeconds);
    AddLine(p010, p000, color, durationSeconds);

    AddLine(p100, p101, color, durationSeconds);
    AddLine(p101, p111, color, durationSeconds);
    AddLine(p111, p110, color, durationSeconds);
    AddLine(p110, p100, color, durationSeconds);

    AddLine(p000, p100, color, durationSeconds);
    AddLine(p001, p101, color, durationSeconds);
    AddLine(p010, p110, color, durationSeconds);
    AddLine(p011, p111, color, durationSeconds);
}

void DebugDraw::AddSphere(const math::Vec3& center, float radius, const math::Vec4& color, float durationSeconds, int segments) {
    if (radius <= 0.0f) return;
    segments = std::max(3, segments);

    const float step = 2.0f * 3.1415926535f / static_cast<float>(segments);

    auto circle = [&](int axisA, int axisB) {
        math::Vec3 prev = center;
        for (int i = 0; i <= segments; ++i) {
            const float t = step * static_cast<float>(i);
            math::Vec3 p = center;
            const float ca = std::cos(t) * radius;
            const float cb = std::sin(t) * radius;
            // write into p[axisA], p[axisB]
            if (axisA == 0) p.x += ca;
            if (axisA == 1) p.y += ca;
            if (axisA == 2) p.z += ca;
            if (axisB == 0) p.x += cb;
            if (axisB == 1) p.y += cb;
            if (axisB == 2) p.z += cb;

            if (i > 0) AddLine(prev, p, color, durationSeconds);
            prev = p;
        }
    };

    circle(0, 1); // XY
    circle(0, 2); // XZ
    circle(1, 2); // YZ
}

void DebugDraw::AddCapsule(const math::Vec3& center, float radius, float height, const math::Vec4& color, float durationSeconds, int segments) {
    if (radius <= 0.0f) return;
    const float half = std::max(0.0f, height * 0.5f);
    const math::Vec3 up(0.0f, 1.0f, 0.0f);
    const math::Vec3 top = center + up * half;
    const math::Vec3 bottom = center - up * half;
    AddCylinder(center, radius, height, color, durationSeconds, segments);
    AddSphere(top, radius, color, durationSeconds, segments);
    AddSphere(bottom, radius, color, durationSeconds, segments);
}

void DebugDraw::AddCylinder(const math::Vec3& center, float radius, float height, const math::Vec4& color, float durationSeconds, int segments) {
    if (radius <= 0.0f) return;
    segments = std::max(3, segments);
    const math::Vec3 up(0.0f, 1.0f, 0.0f);
    const float half = height * 0.5f;
    const math::Vec3 top = center + up * half;
    const math::Vec3 bottom = center - up * half;

    AddCircle(top, up, radius, color, durationSeconds, segments);
    AddCircle(bottom, up, radius, color, durationSeconds, segments);

    math::Vec3 tangent, bitangent;
    BuildBasis(up, tangent, bitangent);
    for (int i = 0; i < segments; ++i) {
        const float t = (2.0f * 3.1415926535f * i) / static_cast<float>(segments);
        const math::Vec3 dir = tangent * std::cos(t) + bitangent * std::sin(t);
        AddLine(bottom + dir * radius, top + dir * radius, color, durationSeconds);
    }
}

void DebugDraw::AddCone(const math::Vec3& center, float radius, float height, const math::Vec4& color, float durationSeconds, int segments) {
    if (radius <= 0.0f || height <= 0.0f) return;
    segments = std::max(3, segments);
    const math::Vec3 up(0.0f, 1.0f, 0.0f);
    const math::Vec3 tip = center + up * (height * 0.5f);
    const math::Vec3 base = center - up * (height * 0.5f);

    AddCircle(base, up, radius, color, durationSeconds, segments);
    math::Vec3 tangent, bitangent;
    BuildBasis(up, tangent, bitangent);
    for (int i = 0; i < segments; ++i) {
        const float t = (2.0f * 3.1415926535f * i) / static_cast<float>(segments);
        const math::Vec3 dir = tangent * std::cos(t) + bitangent * std::sin(t);
        AddLine(base + dir * radius, tip, color, durationSeconds);
    }
}

void DebugDraw::AddPlane(const math::Vec3& center, const math::Vec3& normal, float size, const math::Vec4& color, float durationSeconds) {
    const float half = std::max(0.0f, size * 0.5f);
    math::Vec3 tangent, bitangent;
    BuildBasis(normal, tangent, bitangent);
    const math::Vec3 p0 = center - tangent * half - bitangent * half;
    const math::Vec3 p1 = center + tangent * half - bitangent * half;
    const math::Vec3 p2 = center + tangent * half + bitangent * half;
    const math::Vec3 p3 = center - tangent * half + bitangent * half;
    AddLine(p0, p1, color, durationSeconds);
    AddLine(p1, p2, color, durationSeconds);
    AddLine(p2, p3, color, durationSeconds);
    AddLine(p3, p0, color, durationSeconds);
}

void DebugDraw::AddFrustum(const math::Frustum& frustum, const math::Vec4& color, float durationSeconds) {
    const auto& left = frustum.GetPlane(math::Frustum::Plane::Left);
    const auto& right = frustum.GetPlane(math::Frustum::Plane::Right);
    const auto& bottom = frustum.GetPlane(math::Frustum::Plane::Bottom);
    const auto& top = frustum.GetPlane(math::Frustum::Plane::Top);
    const auto& nearP = frustum.GetPlane(math::Frustum::Plane::Near);
    const auto& farP = frustum.GetPlane(math::Frustum::Plane::Far);

    math::Vec3 nbl = IntersectPlanes(nearP, bottom, left);
    math::Vec3 nbr = IntersectPlanes(nearP, bottom, right);
    math::Vec3 ntr = IntersectPlanes(nearP, top, right);
    math::Vec3 ntl = IntersectPlanes(nearP, top, left);

    math::Vec3 fbl = IntersectPlanes(farP, bottom, left);
    math::Vec3 fbr = IntersectPlanes(farP, bottom, right);
    math::Vec3 ftr = IntersectPlanes(farP, top, right);
    math::Vec3 ftl = IntersectPlanes(farP, top, left);

    AddLine(nbl, nbr, color, durationSeconds);
    AddLine(nbr, ntr, color, durationSeconds);
    AddLine(ntr, ntl, color, durationSeconds);
    AddLine(ntl, nbl, color, durationSeconds);

    AddLine(fbl, fbr, color, durationSeconds);
    AddLine(fbr, ftr, color, durationSeconds);
    AddLine(ftr, ftl, color, durationSeconds);
    AddLine(ftl, fbl, color, durationSeconds);

    AddLine(nbl, fbl, color, durationSeconds);
    AddLine(nbr, fbr, color, durationSeconds);
    AddLine(ntr, ftr, color, durationSeconds);
    AddLine(ntl, ftl, color, durationSeconds);
}

void DebugDraw::AddText(const math::Vec3& position, const std::string& text, const math::Vec4& color, float durationSeconds) {
    DebugText t;
    t.position = position;
    t.text = text;
    t.color = color;
    t.duration = std::max(0.0f, durationSeconds);
    AddTextInternal(t);
}

void DebugDraw::AddGrid(const math::Vec3& center, float size, int divisions, const math::Vec4& color, float durationSeconds) {
    divisions = std::max(1, divisions);
    const float half = std::max(0.0f, size * 0.5f);
    const float step = (half * 2.0f) / static_cast<float>(divisions);
    for (int i = 0; i <= divisions; ++i) {
        const float offset = -half + step * static_cast<float>(i);
        AddLine(center + math::Vec3(offset, 0.0f, -half),
                center + math::Vec3(offset, 0.0f, half),
                color, durationSeconds);
        AddLine(center + math::Vec3(-half, 0.0f, offset),
                center + math::Vec3(half, 0.0f, offset),
                color, durationSeconds);
    }
}

void DebugDraw::AddArrow(const math::Vec3& start, const math::Vec3& end, const math::Vec4& color, float durationSeconds) {
    AddLine(start, end, color, durationSeconds);
    math::Vec3 dir = end - start;
    const float len = glm::length(dir);
    if (len < 1e-4f) return;
    dir /= len;
    math::Vec3 tangent, bitangent;
    BuildBasis(dir, tangent, bitangent);
    const float headLen = std::min(0.25f * len, 0.5f);
    const float headWidth = headLen * 0.5f;
    const math::Vec3 base = end - dir * headLen;
    AddLine(end, base + tangent * headWidth, color, durationSeconds);
    AddLine(end, base - tangent * headWidth, color, durationSeconds);
    AddLine(end, base + bitangent * headWidth, color, durationSeconds);
    AddLine(end, base - bitangent * headWidth, color, durationSeconds);
}

void DebugDraw::AddAxis(const math::Vec3& position, const math::Quat& rotation, float size, float durationSeconds) {
    const math::Vec3 right = rotation * math::Vec3(1.0f, 0.0f, 0.0f);
    const math::Vec3 up = rotation * math::Vec3(0.0f, 1.0f, 0.0f);
    const math::Vec3 forward = rotation * math::Vec3(0.0f, 0.0f, -1.0f);
    AddArrow(position, position + right * size, math::Vec4(1, 0, 0, 1), durationSeconds);
    AddArrow(position, position + up * size, math::Vec4(0, 1, 0, 1), durationSeconds);
    AddArrow(position, position + forward * size, math::Vec4(0, 0, 1, 1), durationSeconds);
}

void DebugDraw::BeginFrame(float dt) {
    if (dt <= 0.0f) return;
    std::scoped_lock lock(g_mutex);
    for (auto& l : g_lines) {
        l.duration -= dt;
    }
    for (auto& t : g_texts) {
        t.duration -= dt;
    }
    g_lines.erase(std::remove_if(g_lines.begin(), g_lines.end(), [](const DebugLine& l) { return l.duration < 0.0f; }), g_lines.end());
    g_texts.erase(std::remove_if(g_texts.begin(), g_texts.end(), [](const DebugText& t) { return t.duration < 0.0f; }), g_texts.end());
}

void DebugDraw::Clear() {
    std::scoped_lock lock(g_mutex);
    g_lines.clear();
    g_texts.clear();
}

std::vector<DebugLine> DebugDraw::GetLines() {
    std::scoped_lock lock(g_mutex);
    return g_lines;
}

std::vector<DebugText> DebugDraw::GetTexts() {
    std::scoped_lock lock(g_mutex);
    return g_texts;
}

} // namespace debug
} // namespace eyelib

