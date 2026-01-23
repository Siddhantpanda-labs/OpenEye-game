#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace debug {

struct DebugLine {
    math::Vec3 a{0.0f, 0.0f, 0.0f};
    math::Vec3 b{0.0f, 0.0f, 0.0f};
    math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float duration = 0.0f; // seconds (0 = this-frame)
};

class DebugDraw {
public:
    // World-space primitives (CPU command buffer; renderer will consume later).
    static void AddLine(const math::Vec3& a, const math::Vec3& b, const math::Vec4& color, float durationSeconds = 0.0f);
    static void AddAABB(const math::Vec3& min, const math::Vec3& max, const math::Vec4& color, float durationSeconds = 0.0f);
    static void AddSphere(const math::Vec3& center, float radius, const math::Vec4& color, float durationSeconds = 0.0f, int segments = 12);

    // Frame management (call once per frame; dt used to expire durations).
    static void BeginFrame(float dt);
    static void Clear();

    // Snapshot for render/tools.
    static std::vector<DebugLine> GetLines();
};

} // namespace debug
} // namespace eyelib

