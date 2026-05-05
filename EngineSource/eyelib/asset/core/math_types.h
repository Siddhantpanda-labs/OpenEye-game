#pragma once

/** Minimal math types for asset. No engine dependency. */

namespace asset {

struct Vec2 {
    float x = 0.f;
    float y = 0.f;
};

struct Vec3 {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

struct Vec4 {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float w = 0.f;
};

/** Row-major 4x4 matrix (matches Assimp aiMatrix4x4 layout). */
struct Mat4 {
    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
};

struct AABB {
    Vec3 min{};
    Vec3 max{};
};

} // namespace asset
