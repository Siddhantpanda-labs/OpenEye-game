#pragma once

#include "eyelib/math/Types.h"
#include <vector>
#include <cstddef>

namespace eyelib {
namespace api {

struct LODSettings {
    std::vector<float> distanceThresholds;
    std::vector<float> hysteresis;
    size_t currentLOD = 0;
    bool enabled = true;
};

class LODManager {
public:
    static size_t UpdateLOD(LODSettings& settings, float distanceFromCamera, size_t maxLOD);
    static float CalculateDistance(const math::Vec3& cameraPos, const math::Vec3& entityPos);
    static LODSettings GetDefaultSettings();
    static LODSettings CreateSettings(const std::vector<float>& thresholds,
                                      const std::vector<float>& hysteresisValues = {});
};

} // namespace api
} // namespace eyelib
