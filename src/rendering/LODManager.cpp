#include "LODManager.h"
#include <algorithm>
#include <cmath>

namespace OpenEye {
namespace Rendering {

size_t LODManager::UpdateLOD(const LODSettings& settings, float distanceFromCamera, size_t maxLOD) {
    if (!settings.enabled || settings.distanceThresholds.empty()) {
        return 0;
    }
    
    size_t newLOD = 0;
    size_t numThresholds = std::min(settings.distanceThresholds.size(), maxLOD);
    
    // Find appropriate LOD level based on distance
    for (size_t i = 0; i < numThresholds; i++) {
        float threshold = settings.distanceThresholds[i];
        float hyst = (i < settings.hysteresis.size()) ? settings.hysteresis[i] : 0.0f;
        
        // Apply hysteresis: switch to higher LOD (lower index) only if we're closer than threshold - hyst
        // Switch to lower LOD (higher index) only if we're farther than threshold + hyst
        size_t currentLOD = settings.currentLOD;
        
        if (currentLOD <= i) {
            // Currently at this LOD or higher detail - check if we should switch to lower detail
            if (distanceFromCamera > threshold + hyst) {
                newLOD = i + 1;
            } else {
                // Stay at current or switch to higher detail
                if (distanceFromCamera < threshold - hyst && i > 0) {
                    newLOD = i - 1;
                    break;
                }
                newLOD = i;
                break;
            }
        } else {
            // Currently at lower detail - check if we should switch to higher detail
            if (distanceFromCamera < threshold - hyst) {
                newLOD = i;
                break;
            } else {
                newLOD = currentLOD;
            }
        }
    }
    
    // Clamp to max LOD
    return std::min(newLOD, maxLOD);
}

float LODManager::CalculateDistance(const glm::vec3& cameraPos, const glm::vec3& entityPos) {
    glm::vec3 diff = entityPos - cameraPos;
    return std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
}

LODSettings LODManager::GetDefaultSettings() {
    return LODSettings();
}

LODSettings LODManager::CreateSettings(const std::vector<float>& thresholds,
                                       const std::vector<float>& hysteresisValues) {
    LODSettings settings;
    settings.distanceThresholds = thresholds;
    
    if (hysteresisValues.empty()) {
        // Default hysteresis: 10% of threshold distance
        settings.hysteresis.resize(thresholds.size());
        for (size_t i = 0; i < thresholds.size(); i++) {
            settings.hysteresis[i] = thresholds[i] * 0.1f;
        }
    } else {
        settings.hysteresis = hysteresisValues;
    }
    
    return settings;
}

} // namespace Rendering
} // namespace OpenEye

