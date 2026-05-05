#pragma once

namespace eyelib {
namespace physics {

/**
 * Jolt global runtime (allocator, Factory, type registration).
 * Must be initialized before creating any physics world; must be shut down
 * once at process exit. Play/Stop must NOT touch these.
 */
void EnsureJoltGlobalsInitialized();
void ShutdownJoltGlobals();

} // namespace physics
} // namespace eyelib
