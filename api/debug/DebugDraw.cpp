#include "DebugDraw.h"

#include "eyelib/debug/DebugDraw.h"

namespace eyelib {
namespace api {
namespace debug {

namespace {
static DebugLine FromInternal(const eyelib::debug::DebugLine& l) {
    DebugLine out;
    out.a = l.a;
    out.b = l.b;
    out.color = l.color;
    out.duration = l.duration;
    return out;
}
} // namespace

void DebugDraw::AddLine(const eyelib::math::Vec3& a, const eyelib::math::Vec3& b, const eyelib::math::Vec4& color, float durationSeconds) {
    eyelib::debug::DebugDraw::AddLine(a, b, color, durationSeconds);
}

void DebugDraw::AddAABB(const eyelib::math::Vec3& min, const eyelib::math::Vec3& max, const eyelib::math::Vec4& color, float durationSeconds) {
    eyelib::debug::DebugDraw::AddAABB(min, max, color, durationSeconds);
}

void DebugDraw::AddSphere(const eyelib::math::Vec3& center, float radius, const eyelib::math::Vec4& color, float durationSeconds, int segments) {
    eyelib::debug::DebugDraw::AddSphere(center, radius, color, durationSeconds, segments);
}

void DebugDraw::BeginFrame(float dt) {
    eyelib::debug::DebugDraw::BeginFrame(dt);
}

void DebugDraw::Clear() {
    eyelib::debug::DebugDraw::Clear();
}

std::vector<DebugLine> DebugDraw::GetLines() {
    auto lines = eyelib::debug::DebugDraw::GetLines();
    std::vector<DebugLine> out;
    out.reserve(lines.size());
    for (const auto& l : lines) out.push_back(FromInternal(l));
    return out;
}

} // namespace debug
} // namespace api
} // namespace eyelib

