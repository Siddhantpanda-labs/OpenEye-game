#pragma once

namespace eyelib {
namespace world {
namespace components {

/**
 * XR Camera: entity that drives VR/stereo rendering when an XR session is active.
 * View count, pose, and FOV come from the XR provider (e.g. OpenXR); this component
 * marks the entity and holds XR-specific settings.
 */
struct XRCamera {
    /** When true, this XR camera is used for rendering when the XR session is active. */
    bool isPrimary = true;
    /** Near plane for projection (used when building view from XR FOV). */
    float nearPlane = 0.1f;
    /** Far plane for projection. */
    float farPlane = 1000.0f;
};

} // namespace components
} // namespace world
} // namespace eyelib
