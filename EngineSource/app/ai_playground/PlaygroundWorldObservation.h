#pragma once

#include <cstddef>
#include <string>

namespace eyelib {
namespace api {
namespace world {
class World;
}
} // namespace api
} // namespace eyelib

namespace openeye::ai_playground {

/**
 * Builds a bounded JSON snapshot of the world for LLM context (Phase 1).
 * Uses GetComponentJson only — no direct eyelib registry access.
 */
std::string BuildWorldObservationJson(eyelib::api::world::World& world,
                                        size_t maxEntities = 128,
                                        size_t maxCharsPerComponent = 4096);

} // namespace openeye::ai_playground
