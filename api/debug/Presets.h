#pragma once

namespace eyelib {
namespace api {
namespace debug {

// Simple façade over engine presets.
struct Presets {
    static void SetWireframe(bool enabled);
    static bool GetWireframe();

    static void SetPhysicsDebugDraw(bool enabled);
    static bool GetPhysicsDebugDraw();

    static void SetFrameTimeGraph(bool enabled);
    static bool GetFrameTimeGraph();
};

} // namespace debug
} // namespace api
} // namespace eyelib

