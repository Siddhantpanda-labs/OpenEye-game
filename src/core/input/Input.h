#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>

namespace OpenEye {
namespace Core {

class Input {
public:
    static bool IsKeyPressed(int key);
    static bool IsKeyJustPressed(int key);
    static bool IsKeyJustReleased(int key);
    
    static bool IsMouseButtonPressed(int button);
    static bool IsMouseButtonJustPressed(int button);
    static bool IsMouseButtonJustReleased(int button);
    
    static glm::vec2 GetMousePosition();
    static glm::vec2 GetMouseDelta();
    static float GetMouseScrollDelta();
    
    // Called by Window at the start of each frame
    static void Update();
    
    // Called by Window when key events occur
    static void OnKeyEvent(int key, int scancode, int action, int mods);
    static void OnMouseButtonEvent(int button, int action, int mods);
    static void OnMouseMoveEvent(double x, double y);
    static void OnScrollEvent(double xoffset, double yoffset);

private:
    static constexpr int MAX_KEYS = 512;
    static constexpr int MAX_MOUSE_BUTTONS = 8;
    
    static bool s_keys[MAX_KEYS];
    static bool s_keysLastFrame[MAX_KEYS];
    static bool s_mouseButtons[MAX_MOUSE_BUTTONS];
    static bool s_mouseButtonsLastFrame[MAX_MOUSE_BUTTONS];
    
    static glm::vec2 s_mousePosition;
    static glm::vec2 s_mousePositionLastFrame;
    static float s_scrollDelta;
};

} // namespace Core
} // namespace OpenEye

