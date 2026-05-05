#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/Frustum.h"

#include <string>
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

struct DebugText {
    eyelib::math::Vec3 position{0.0f, 0.0f, 0.0f};
    std::string text{};
    eyelib::math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float duration = 0.0f;
};

class DebugDraw {
public:
    static void AddLine(const eyelib::math::Vec3& a, const eyelib::math::Vec3& b, const eyelib::math::Vec4& color, float durationSeconds = 0.0f);
    static void AddAABB(const eyelib::math::Vec3& min, const eyelib::math::Vec3& max, const eyelib::math::Vec4& color, float durationSeconds = 0.0f);
    static void AddSphere(const eyelib::math::Vec3& center, float radius, const eyelib::math::Vec4& color, float durationSeconds = 0.0f, int segments = 12);
    static void AddCapsule(const eyelib::math::Vec3& center, float radius, float height, const eyelib::math::Vec4& color, float durationSeconds = 0.0f, int segments = 12);
    static void AddCylinder(const eyelib::math::Vec3& center, float radius, float height, const eyelib::math::Vec4& color, float durationSeconds = 0.0f, int segments = 12);
    static void AddCone(const eyelib::math::Vec3& center, float radius, float height, const eyelib::math::Vec4& color, float durationSeconds = 0.0f, int segments = 12);
    static void AddPlane(const eyelib::math::Vec3& center, const eyelib::math::Vec3& normal, float size, const eyelib::math::Vec4& color, float durationSeconds = 0.0f);
    static void AddFrustum(const eyelib::math::Frustum& frustum, const eyelib::math::Vec4& color, float durationSeconds = 0.0f);
    static void AddText(const eyelib::math::Vec3& position, const std::string& text, const eyelib::math::Vec4& color, float durationSeconds = 0.0f);
    static void AddGrid(const eyelib::math::Vec3& center, float size, int divisions, const eyelib::math::Vec4& color, float durationSeconds = 0.0f);
    static void AddArrow(const eyelib::math::Vec3& start, const eyelib::math::Vec3& end, const eyelib::math::Vec4& color, float durationSeconds = 0.0f);
    static void AddAxis(const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation, float size, float durationSeconds = 0.0f);

    static void BeginFrame(float dt);
    static void Clear();

    static std::vector<DebugLine> GetLines();
    static std::vector<DebugText> GetTexts();
};

} // namespace debug
} // namespace api
} // namespace eyelib

