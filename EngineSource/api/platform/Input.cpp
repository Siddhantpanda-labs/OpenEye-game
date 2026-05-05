#include "api/platform/Input.h"

#include "eyelib/platform/input/Input.h"

namespace eyelib {
namespace api {

bool Input::IsKeyPressed(int key) {
    return eyelib::platform::Input::IsKeyPressed(key);
}

bool Input::IsKeyJustPressed(int key) {
    return eyelib::platform::Input::IsKeyJustPressed(key);
}

bool Input::IsKeyJustReleased(int key) {
    return eyelib::platform::Input::IsKeyJustReleased(key);
}

bool Input::IsMouseButtonPressed(int button) {
    return eyelib::platform::Input::IsMouseButtonPressed(button);
}

bool Input::IsMouseButtonJustPressed(int button) {
    return eyelib::platform::Input::IsMouseButtonJustPressed(button);
}

bool Input::IsMouseButtonJustReleased(int button) {
    return eyelib::platform::Input::IsMouseButtonJustReleased(button);
}

Vec2f Input::MousePosition() {
    auto p = eyelib::platform::Input::MousePosition();
    return Vec2f{ p.x, p.y };
}

Vec2f Input::MouseDelta() {
    auto d = eyelib::platform::Input::MouseDelta();
    return Vec2f{ d.x, d.y };
}

float Input::ScrollDelta() {
    return eyelib::platform::Input::ScrollDelta();
}

void Input::Update() {
    eyelib::platform::Input::Update();
}

} // namespace api
} // namespace eyelib

