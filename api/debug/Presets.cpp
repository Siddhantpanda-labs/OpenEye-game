#include "Presets.h"

#include "eyelib/debug/Presets.h"

namespace eyelib {
namespace api {
namespace debug {

void Presets::SetWireframe(bool enabled) { eyelib::debug::GetPresets().wireframe.store(enabled); }
bool Presets::GetWireframe() { return eyelib::debug::GetPresets().wireframe.load(); }

void Presets::SetPhysicsDebugDraw(bool enabled) { eyelib::debug::GetPresets().physicsDebugDraw.store(enabled); }
bool Presets::GetPhysicsDebugDraw() { return eyelib::debug::GetPresets().physicsDebugDraw.load(); }

void Presets::SetFrameTimeGraph(bool enabled) { eyelib::debug::GetPresets().frameTimeGraph.store(enabled); }
bool Presets::GetFrameTimeGraph() { return eyelib::debug::GetPresets().frameTimeGraph.load(); }

void Presets::SetDebugDrawEnabled(bool enabled) { eyelib::debug::GetPresets().debugDrawEnabled.store(enabled); }
bool Presets::GetDebugDrawEnabled() { return eyelib::debug::GetPresets().debugDrawEnabled.load(); }

} // namespace debug
} // namespace api
} // namespace eyelib

