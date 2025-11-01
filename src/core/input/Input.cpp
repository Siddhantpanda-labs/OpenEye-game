#include "Input.h"
#include <cstring>

namespace OpenEye {
namespace Core {

bool Input::s_keys[MAX_KEYS] = {false};
bool Input::s_keysLastFrame[MAX_KEYS] = {false};
bool Input::s_mouseButtons[MAX_MOUSE_BUTTONS] = {false};
bool Input::s_mouseButtonsLastFrame[MAX_MOUSE_BUTTONS] = {false};
glm::vec2 Input::s_mousePosition = glm::vec2(0.0f);
glm::vec2 Input::s_mousePositionLastFrame = glm::vec2(0.0f);
float Input::s_scrollDelta = 0.0f;

bool Input::IsKeyPressed(int key) {
    if (key < 0 || key >= MAX_KEYS) return false;
    return s_keys[key];
}

bool Input::IsKeyJustPressed(int key) {
    if (key < 0 || key >= MAX_KEYS) return false;
    return s_keys[key] && !s_keysLastFrame[key];
}

bool Input::IsKeyJustReleased(int key) {
    if (key < 0 || key >= MAX_KEYS) return false;
    return !s_keys[key] && s_keysLastFrame[key];
}

bool Input::IsMouseButtonPressed(int button) {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    return s_mouseButtons[button];
}

bool Input::IsMouseButtonJustPressed(int button) {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    return s_mouseButtons[button] && !s_mouseButtonsLastFrame[button];
}

bool Input::IsMouseButtonJustReleased(int button) {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    return !s_mouseButtons[button] && s_mouseButtonsLastFrame[button];
}

glm::vec2 Input::GetMousePosition() {
    return s_mousePosition;
}

glm::vec2 Input::GetMouseDelta() {
    return s_mousePosition - s_mousePositionLastFrame;
}

float Input::GetMouseScrollDelta() {
    return s_scrollDelta;
}

void Input::Update() {
    // Copy current frame state to last frame
    std::memcpy(s_keysLastFrame, s_keys, sizeof(s_keys));
    std::memcpy(s_mouseButtonsLastFrame, s_mouseButtons, sizeof(s_mouseButtons));
    s_mousePositionLastFrame = s_mousePosition;
    
    // Reset scroll delta (it's only valid for one frame)
    s_scrollDelta = 0.0f;
}

void Input::OnKeyEvent(int key, int /*scancode*/, int action, int /*mods*/) {
    if (key < 0 || key >= MAX_KEYS) return;
    
    if (action == GLFW_PRESS) {
        s_keys[key] = true;
    } else if (action == GLFW_RELEASE) {
        s_keys[key] = false;
    }
    // GLFW_REPEAT is handled as pressed
}

void Input::OnMouseButtonEvent(int button, int action, int /*mods*/) {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return;
    
    if (action == GLFW_PRESS) {
        s_mouseButtons[button] = true;
    } else if (action == GLFW_RELEASE) {
        s_mouseButtons[button] = false;
    }
}

void Input::OnMouseMoveEvent(double x, double y) {
    s_mousePosition.x = static_cast<float>(x);
    s_mousePosition.y = static_cast<float>(y);
}

void Input::OnScrollEvent(double /*xoffset*/, double yoffset) {
    s_scrollDelta = static_cast<float>(yoffset);
}

} // namespace Core
} // namespace OpenEye

