#include "api/render/LODManager.h"
#include "eyelib/render/LODManager.h"

namespace eyelib {
namespace api {

namespace {
::eyelib::render::LODSettings ToInternal(const LODSettings& settings) {
    ::eyelib::render::LODSettings out;
    out.distanceThresholds = settings.distanceThresholds;
    out.hysteresis = settings.hysteresis;
    out.currentLOD = settings.currentLOD;
    out.enabled = settings.enabled;
    return out;
}

LODSettings FromInternal(const ::eyelib::render::LODSettings& settings) {
    LODSettings out;
    out.distanceThresholds = settings.distanceThresholds;
    out.hysteresis = settings.hysteresis;
    out.currentLOD = settings.currentLOD;
    out.enabled = settings.enabled;
    return out;
}
} // namespace

size_t LODManager::UpdateLOD(LODSettings& settings, float distanceFromCamera, size_t maxLOD) {
    auto internal = ToInternal(settings);
    auto result = ::eyelib::render::LODManager::UpdateLOD(internal, distanceFromCamera, maxLOD);
    settings = FromInternal(internal);
    return result;
}

float LODManager::CalculateDistance(const math::Vec3& cameraPos, const math::Vec3& entityPos) {
    return ::eyelib::render::LODManager::CalculateDistance(cameraPos, entityPos);
}

LODSettings LODManager::GetDefaultSettings() {
    return FromInternal(::eyelib::render::LODManager::GetDefaultSettings());
}

LODSettings LODManager::CreateSettings(const std::vector<float>& thresholds,
                                       const std::vector<float>& hysteresisValues) {
    return FromInternal(::eyelib::render::LODManager::CreateSettings(thresholds, hysteresisValues));
}

} // namespace api
} // namespace eyelib
