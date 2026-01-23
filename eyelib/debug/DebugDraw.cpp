#include "DebugDraw.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace eyelib {
namespace debug {

namespace {
std::mutex g_mutex;
std::vector<DebugLine> g_lines;

static void AddLineInternal(const DebugLine& l) {
    std::scoped_lock lock(g_mutex);
    g_lines.push_back(l);
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

void DebugDraw::BeginFrame(float dt) {
    if (dt <= 0.0f) return;
    std::scoped_lock lock(g_mutex);
    for (auto& l : g_lines) {
        if (l.duration > 0.0f) l.duration -= dt;
    }
    g_lines.erase(std::remove_if(g_lines.begin(), g_lines.end(), [](const DebugLine& l) { return l.duration < 0.0f; }), g_lines.end());
}

void DebugDraw::Clear() {
    std::scoped_lock lock(g_mutex);
    g_lines.clear();
}

std::vector<DebugLine> DebugDraw::GetLines() {
    std::scoped_lock lock(g_mutex);
    return g_lines;
}

} // namespace debug
} // namespace eyelib

