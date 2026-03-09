#include "platform/input/Input.h"

#include <cstring>

// GLFW action constants (we avoid including GLFW in the header; cpp is fine to include it)
#include <GLFW/glfw3.h>

namespace eyelib {
namespace platform {

bool Input::s_keys[Input::kMaxKeys] = { false };
bool Input::s_keysLastFrame[Input::kMaxKeys] = { false };
bool Input::s_mouseButtons[Input::kMaxMouseButtons] = { false };
bool Input::s_mouseButtonsLastFrame[Input::kMaxMouseButtons] = { false };
Vec2f Input::s_mousePos = {};
Vec2f Input::s_mousePosLastFrame = {};
float Input::s_scrollDelta = 0.0f;

bool Input::IsKeyPressed(int key) {
    if (key < 0 || key >= kMaxKeys) return false;
    return s_keys[key];
}

bool Input::IsKeyJustPressed(int key) {
    if (key < 0 || key >= kMaxKeys) return false;
    return s_keys[key] && !s_keysLastFrame[key];
}

bool Input::IsKeyJustReleased(int key) {
    if (key < 0 || key >= kMaxKeys) return false;
    return !s_keys[key] && s_keysLastFrame[key];
}

bool Input::IsMouseButtonPressed(int button) {
    if (button < 0 || button >= kMaxMouseButtons) return false;
    return s_mouseButtons[button];
}

bool Input::IsMouseButtonJustPressed(int button) {
    if (button < 0 || button >= kMaxMouseButtons) return false;
    return s_mouseButtons[button] && !s_mouseButtonsLastFrame[button];
}

bool Input::IsMouseButtonJustReleased(int button) {
    if (button < 0 || button >= kMaxMouseButtons) return false;
    return !s_mouseButtons[button] && s_mouseButtonsLastFrame[button];
}

Vec2f Input::MousePosition() {
    return s_mousePos;
}

Vec2f Input::MouseDelta() {
    return Vec2f{
        s_mousePos.x - s_mousePosLastFrame.x,
        s_mousePos.y - s_mousePosLastFrame.y
    };
}

float Input::ScrollDelta() {
    return s_scrollDelta;
}

void Input::Update() {
    std::memcpy(s_keysLastFrame, s_keys, sizeof(s_keys));
    std::memcpy(s_mouseButtonsLastFrame, s_mouseButtons, sizeof(s_mouseButtons));
    s_mousePosLastFrame = s_mousePos;
    s_scrollDelta = 0.0f;
}

void Input::OnKeyEvent(int key, int /*scancode*/, int action, int /*mods*/) {
    if (key < 0 || key >= kMaxKeys) return;

    if (action == GLFW_PRESS) {
        s_keys[key] = true;
    } else if (action == GLFW_RELEASE) {
        s_keys[key] = false;
    }
}

void Input::OnMouseButtonEvent(int button, int action, int /*mods*/) {
    if (button < 0 || button >= kMaxMouseButtons) return;

    if (action == GLFW_PRESS) {
        s_mouseButtons[button] = true;
    } else if (action == GLFW_RELEASE) {
        s_mouseButtons[button] = false;
    }
}

void Input::OnMouseMoveEvent(double x, double y) {
    s_mousePos.x = static_cast<float>(x);
    s_mousePos.y = static_cast<float>(y);
}

void Input::OnScrollEvent(double /*xoffset*/, double yoffset) {
    s_scrollDelta = static_cast<float>(yoffset);
}

} // namespace platform
} // namespace eyelib

