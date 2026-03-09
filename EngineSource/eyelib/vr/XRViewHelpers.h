#pragma once

#include "eyelib/vr/XRTypes.h"
#include "eyelib/math/Types.h"
#include "eyelib/math/TransformUtils.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace eyelib {
namespace vr {

/** View/projection from XR (no dependency on render module). Convert to render::CameraView at use site. */
struct XRViewProjection {
    math::Mat4 view{1.0f};
    math::Mat4 projection{1.0f};
    math::Vec3 viewPos{0, 0, 0};
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

/** Default interpupillary distance for stereo preview (meters). */
constexpr float kDefaultStereoIPD = 0.064f;
/** Default horizontal FOV half-angle (radians) for stereo preview when no XR runtime. */
constexpr float kDefaultStereoFovHalfX = 0.5f * (90.f * 3.14159265f / 180.f);
/** Default vertical FOV half-angle (radians). */
constexpr float kDefaultStereoFovHalfY = 0.5f * (90.f * 3.14159265f / 180.f);

/** Fill left and right XRViewData for stereo preview from center pose (no headset). */
inline void StereoViewDataFromCenterPose(const math::Vec3& position, const math::Quat& orientation,
                                         float ipd, float fovHalfXRad, float fovHalfYRad,
                                         XRViewData& outLeft, XRViewData& outRight) {
    const math::Vec3 rightDir = orientation * math::Vec3(1.f, 0.f, 0.f);
    const float halfIpd = ipd * 0.5f;
    outLeft.position = position - halfIpd * rightDir;
    outLeft.orientation = orientation;
    outLeft.angleLeft = -fovHalfXRad;
    outLeft.angleRight = fovHalfXRad;
    outLeft.angleUp = fovHalfYRad;
    outLeft.angleDown = -fovHalfYRad;

    outRight.position = position + halfIpd * rightDir;
    outRight.orientation = orientation;
    outRight.angleLeft = -fovHalfXRad;
    outRight.angleRight = fovHalfXRad;
    outRight.angleUp = fovHalfYRad;
    outRight.angleDown = -fovHalfYRad;
}

/** Build view and projection from XR view pose and FOV. Uses asymmetric frustum for VR. */
inline XRViewProjection ViewProjectionFromXRView(const XRViewData& view, float nearPlane, float farPlane) {
    XRViewProjection out;
    out.viewPos = view.position;
    out.nearPlane = nearPlane;
    out.farPlane = farPlane;

    math::Mat4 rot = glm::mat4_cast(view.orientation);
    math::Mat4 invRot = glm::transpose(rot);
    math::Mat4 invTrans = glm::translate(math::Mat4(1.0f), -view.position);
    out.view = invRot * invTrans;

    float tanL = std::tan(view.angleLeft);
    float tanR = std::tan(view.angleRight);
    float tanD = std::tan(view.angleDown);
    float tanU = std::tan(view.angleUp);
    out.projection = glm::frustum(tanL * nearPlane, tanR * nearPlane,
                                  tanD * nearPlane, tanU * nearPlane,
                                  nearPlane, farPlane);
    return out;
}

} // namespace vr
} // namespace eyelib
