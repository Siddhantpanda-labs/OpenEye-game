#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace eyelib {
namespace world {
namespace skybox {

/** Serialized and in-memory id for the procedural sky pass (no cubemap). */
constexpr const char kProceduralId[] = "procedural";

/** Default horizontal-cross cubemap path (editor convenience / templates). */
constexpr const char kDefaultEveningCrossPath[] = "Skybox/SkyMap/Cubemap_Sky_512x512.png";

/** Inspector / CLI alias: expands to kDefaultEveningCrossPath when setting skybox. */
constexpr const char kEveningAlias[] = "evening";

/** Cross layout face size for kDefaultEveningCrossPath (512 px per face). */
constexpr uint32_t kDefaultCrossFaceSize = 512u;

bool IsProceduralSkyboxId(std::string_view id);

/** Trim ASCII whitespace; map empty / "procedural" / "evening" to canonical stored ids. */
std::string NormalizeSkyboxAssetId(std::string_view input);

} // namespace skybox
} // namespace world
} // namespace eyelib
