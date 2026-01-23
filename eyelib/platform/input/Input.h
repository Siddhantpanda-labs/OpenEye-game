#pragma once

#include <cstdint>

namespace eyelib {
namespace platform {

/**
 * @brief Lightweight 2D float vector for platform input reporting.
 *
 * Kept here to avoid depending on glm (and until eyelib/math is in place).
 */
struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

/**
 * @brief Central input state (keyboard/mouse) updated via window callbacks.
 *
 * Notes:
 * - Uses integer key/button codes (currently GLFW-style codes).
 * - Does not expose GLFW types in the header.
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

    /** Call once per frame to advance "just pressed/released" state. */
    static void Update();

    // Event ingestion (called by Window)
    static void OnKeyEvent(int key, int scancode, int action, int mods);
    static void OnMouseButtonEvent(int button, int action, int mods);
    static void OnMouseMoveEvent(double x, double y);
    static void OnScrollEvent(double xoffset, double yoffset);

private:
    static constexpr int kMaxKeys = 512;
    static constexpr int kMaxMouseButtons = 16;

    static bool s_keys[kMaxKeys];
    static bool s_keysLastFrame[kMaxKeys];
    static bool s_mouseButtons[kMaxMouseButtons];
    static bool s_mouseButtonsLastFrame[kMaxMouseButtons];

    static Vec2f s_mousePos;
    static Vec2f s_mousePosLastFrame;
    static float s_scrollDelta;
};

} // namespace platform
} // namespace eyelib

