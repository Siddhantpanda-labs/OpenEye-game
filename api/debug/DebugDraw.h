#pragma once

#include "eyelib/math/Types.h"

#include <vector>

namespace eyelib {
namespace api {
namespace debug {

struct DebugLine {
    eyelib::math::Vec3 a{0.0f, 0.0f, 0.0f};
    eyelib::math::Vec3 b{0.0f, 0.0f, 0.0f};
    eyelib::math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float duration = 0.0f;
};

class DebugDraw {
public:
    static void AddLine(const eyelib::math::Vec3& a, const eyelib::math::Vec3& b, const eyelib::math::Vec4& color, float durationSeconds = 0.0f);
    static void AddAABB(const eyelib::math::Vec3& min, const eyelib::math::Vec3& max, const eyelib::math::Vec4& color, float durationSeconds = 0.0f);
    static void AddSphere(const eyelib::math::Vec3& center, float radius, const eyelib::math::Vec4& color, float durationSeconds = 0.0f, int segments = 12);

    static void BeginFrame(float dt);
    static void Clear();

    static std::vector<DebugLine> GetLines();
};

} // namespace debug
} // namespace api
} // namespace eyelib

