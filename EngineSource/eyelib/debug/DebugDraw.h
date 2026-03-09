#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/Frustum.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace debug {

struct DebugLine {
    math::Vec3 a{0.0f, 0.0f, 0.0f};
    math::Vec3 b{0.0f, 0.0f, 0.0f};
    math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float duration = 0.0f; // seconds (0 = this-frame)
};

struct DebugText {
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    std::string text{};
    math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float duration = 0.0f;
};

class DebugDraw {
public:
    // World-space primitives (CPU command buffer; renderer will consume later).
    static void AddLine(const math::Vec3& a, const math::Vec3& b, const math::Vec4& color, float durationSeconds = 0.0f);
    static void AddAABB(const math::Vec3& min, const math::Vec3& max, const math::Vec4& color, float durationSeconds = 0.0f);
    static void AddSphere(const math::Vec3& center, float radius, const math::Vec4& color, float durationSeconds = 0.0f, int segments = 12);
    static void AddCapsule(const math::Vec3& center, float radius, float height, const math::Vec4& color, float durationSeconds = 0.0f, int segments = 12);
    static void AddCylinder(const math::Vec3& center, float radius, float height, const math::Vec4& color, float durationSeconds = 0.0f, int segments = 12);
    static void AddCone(const math::Vec3& center, float radius, float height, const math::Vec4& color, float durationSeconds = 0.0f, int segments = 12);
    static void AddPlane(const math::Vec3& center, const math::Vec3& normal, float size, const math::Vec4& color, float durationSeconds = 0.0f);
    static void AddFrustum(const math::Frustum& frustum, const math::Vec4& color, float durationSeconds = 0.0f);
    static void AddText(const math::Vec3& position, const std::string& text, const math::Vec4& color, float durationSeconds = 0.0f);
    static void AddGrid(const math::Vec3& center, float size, int divisions, const math::Vec4& color, float durationSeconds = 0.0f);
    static void AddArrow(const math::Vec3& start, const math::Vec3& end, const math::Vec4& color, float durationSeconds = 0.0f);
    static void AddAxis(const math::Vec3& position, const math::Quat& rotation, float size, float durationSeconds = 0.0f);

    // Frame management (call once per frame; dt used to expire durations).
    static void BeginFrame(float dt);
    static void Clear();

    // Snapshot for render/tools.
    static std::vector<DebugLine> GetLines();
    static std::vector<DebugText> GetTexts();
};

} // namespace debug
} // namespace eyelib

