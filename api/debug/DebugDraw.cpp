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

static DebugText FromInternalText(const eyelib::debug::DebugText& t) {
    DebugText out;
    out.position = t.position;
    out.text = t.text;
    out.color = t.color;
    out.duration = t.duration;
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

void DebugDraw::AddCapsule(const eyelib::math::Vec3& center, float radius, float height, const eyelib::math::Vec4& color, float durationSeconds, int segments) {
    eyelib::debug::DebugDraw::AddCapsule(center, radius, height, color, durationSeconds, segments);
}

void DebugDraw::AddCylinder(const eyelib::math::Vec3& center, float radius, float height, const eyelib::math::Vec4& color, float durationSeconds, int segments) {
    eyelib::debug::DebugDraw::AddCylinder(center, radius, height, color, durationSeconds, segments);
}

void DebugDraw::AddCone(const eyelib::math::Vec3& center, float radius, float height, const eyelib::math::Vec4& color, float durationSeconds, int segments) {
    eyelib::debug::DebugDraw::AddCone(center, radius, height, color, durationSeconds, segments);
}

void DebugDraw::AddPlane(const eyelib::math::Vec3& center, const eyelib::math::Vec3& normal, float size, const eyelib::math::Vec4& color, float durationSeconds) {
    eyelib::debug::DebugDraw::AddPlane(center, normal, size, color, durationSeconds);
}

void DebugDraw::AddFrustum(const eyelib::math::Frustum& frustum, const eyelib::math::Vec4& color, float durationSeconds) {
    eyelib::debug::DebugDraw::AddFrustum(frustum, color, durationSeconds);
}

void DebugDraw::AddText(const eyelib::math::Vec3& position, const std::string& text, const eyelib::math::Vec4& color, float durationSeconds) {
    eyelib::debug::DebugDraw::AddText(position, text, color, durationSeconds);
}

void DebugDraw::AddGrid(const eyelib::math::Vec3& center, float size, int divisions, const eyelib::math::Vec4& color, float durationSeconds) {
    eyelib::debug::DebugDraw::AddGrid(center, size, divisions, color, durationSeconds);
}

void DebugDraw::AddArrow(const eyelib::math::Vec3& start, const eyelib::math::Vec3& end, const eyelib::math::Vec4& color, float durationSeconds) {
    eyelib::debug::DebugDraw::AddArrow(start, end, color, durationSeconds);
}

void DebugDraw::AddAxis(const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation, float size, float durationSeconds) {
    eyelib::debug::DebugDraw::AddAxis(position, rotation, size, durationSeconds);
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

std::vector<DebugText> DebugDraw::GetTexts() {
    auto texts = eyelib::debug::DebugDraw::GetTexts();
    std::vector<DebugText> out;
    out.reserve(texts.size());
    for (const auto& t : texts) out.push_back(FromInternalText(t));
    return out;
}

} // namespace debug
} // namespace api
} // namespace eyelib

