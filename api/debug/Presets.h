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

    /** Enable/disable debug draw (lines, text from DebugDraw). When enabled, skeleton/debug lines are visible. */
    static void SetDebugDrawEnabled(bool enabled);
    static bool GetDebugDrawEnabled();
};

} // namespace debug
} // namespace api
} // namespace eyelib

