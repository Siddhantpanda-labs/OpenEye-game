#pragma once

#include "eyelib/math/Types.h"
#include <vector>
#include <cstddef>

namespace eyelib {
namespace render {

/**
 * @brief LOD settings for an entity.
 * Distance thresholds (furthest first), hysteresis to prevent flickering.
 */
struct LODSettings {
    std::vector<float> distanceThresholds;  // Distance at which to switch LOD (furthest first)
    std::vector<float> hysteresis;          // Hysteresis to prevent flickering (in distance units)
    size_t currentLOD = 0;                  // Current active LOD level
    bool enabled = true;

    LODSettings() {
        // Default: 2 LODs
        distanceThresholds = { 50.0f, 100.0f };  // Switch at 50m, then 100m
        hysteresis = { 5.0f, 10.0f };            // 5m and 10m hysteresis
    }
};

/**
 * @brief Manages LOD switching with hysteresis to prevent popping.
 */
class LODManager {
public:
    /**
     * @brief Update LOD for an entity based on distance from camera.
     * @param settings LOD settings (currentLOD is updated)
     * @param distanceFromCamera Distance from camera to entity
     * @param maxLOD Maximum LOD index available
     * @return The active LOD index (0 = highest detail)
     */
    static size_t UpdateLOD(LODSettings& settings, float distanceFromCamera, size_t maxLOD);

    /**
     * @brief Calculate distance from camera position to entity.
     */
    static float CalculateDistance(const math::Vec3& cameraPos, const math::Vec3& entityPos);

    /**
     * @brief Get default LOD settings.
     */
    static LODSettings GetDefaultSettings();

    /**
     * @brief Create LOD settings with custom thresholds.
     * @param thresholds Distance thresholds (furthest first)
     * @param hysteresisValues Optional hysteresis values (defaults to 10% of threshold)
     */
    static LODSettings CreateSettings(const std::vector<float>& thresholds,
                                      const std::vector<float>& hysteresisValues = {});
};

} // namespace render
} // namespace eyelib
