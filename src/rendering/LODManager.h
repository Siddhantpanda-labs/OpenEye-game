#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace OpenEye {
namespace Rendering {

// LOD (Level of Detail) settings for an entity
struct LODSettings {
    std::vector<float> distanceThresholds; // Distance at which to switch LOD (furthest first)
    std::vector<float> hysteresis;          // Hysteresis to prevent flickering (in distance units)
    size_t currentLOD = 0;                  // Current active LOD level
    bool enabled = true;
    
    LODSettings() {
        // Default: 2 LODs
        distanceThresholds = { 50.0f, 100.0f }; // Switch at 50m, then 100m
        hysteresis = { 5.0f, 10.0f };           // 5m and 10m hysteresis
    }
};

// Manages LOD switching with hysteresis to prevent popping
class LODManager {
public:
    // Update LOD for an entity based on distance from camera
    // Returns the active LOD index (0 = highest detail)
    static size_t UpdateLOD(const LODSettings& settings, float distanceFromCamera, size_t maxLOD);
    
    // Calculate distance from camera position to entity
    static float CalculateDistance(const glm::vec3& cameraPos, const glm::vec3& entityPos);
    
    // Get default LOD settings
    static LODSettings GetDefaultSettings();
    
    // Create LOD settings with custom thresholds
    static LODSettings CreateSettings(const std::vector<float>& thresholds,
                                      const std::vector<float>& hysteresisValues = {});
};

} // namespace Rendering
} // namespace OpenEye

