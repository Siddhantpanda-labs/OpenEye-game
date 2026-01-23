#pragma once

namespace eyelib {
namespace api {

struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

/**
 * @brief Client-facing input queries.
 *
 * Uses integer key/button codes (currently GLFW-style codes) but does not
 * expose GLFW headers.
 */
class Input {
public:
    static bool IsKeyPressed(int key);
    static bool IsKeyJustPressed(int key);
    static bool IsKeyJustReleased(int key);

    static bool IsMouseButtonPressed(int button);
    static bool IsMouseButtonJustPressed(int button);
    static bool IsMouseButtonJustReleased(int button);

    static Vec2f MousePosition();
    static Vec2f MouseDelta();
    static float ScrollDelta();
};

} // namespace api
} // namespace eyelib

