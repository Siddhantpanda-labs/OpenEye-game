#pragma once

#include <atomic>

namespace eyelib {
namespace debug {

/**
 * @brief Debug/diagnostic toggles (engine-side).
 *
 * These are intentionally renderer-agnostic. Render/physics integrations will
 * consult these flags when they’re implemented.
 */
struct Presets {
    std::atomic<bool> wireframe{false};
    std::atomic<bool> physicsDebugDraw{false};
    std::atomic<bool> frameTimeGraph{true};
};

Presets& GetPresets();

} // namespace debug
} // namespace eyelib

