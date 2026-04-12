#pragma once

#include "eyelib/rendering/scene/CameraView.h"
#include "eyelib/rendering/core/RenderBackend.h"
#include "eyelib/math/TransformUtils.h"

#include <cmath>

namespace eyelib {
namespace rendering {
namespace camera {

enum class DepthRange {
    NegOneToOne,
    ZeroToOne
};

/**
 * Authoritative camera clip-space policy for a backend.
 * EyelibRendering is Vulkan-only; Default matches Vulkan clip conventions here.
 */
struct CameraPolicy {
    DepthRange depthRange = DepthRange::NegOneToOne;
    bool flipClipY = false;
    bool reverseZ = false;
};

inline CameraPolicy PolicyForBackend(RenderBackend backend) {
    CameraPolicy p{};
    switch (backend) {
        case RenderBackend::Vulkan:
        case RenderBackend::Default:
        default:
            p.depthRange = DepthRange::ZeroToOne;
            p.flipClipY = false;
            p.reverseZ = false;
            break;
    }
    return p;
}

struct FreeFlyCameraDesc {
    math::Vec3 position{0.0f, 0.0f, 10.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fovDegrees = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

inline math::Vec3 ComputeForward(float yaw, float pitch) {
    const float cy = std::cos(pitch);
    const float sy = std::sin(pitch);
    const float ct = std::cos(yaw);
    const float st = std::sin(yaw);
    return math::Vec3(cy * st, sy, -cy * ct);
}

inline math::Mat4 BuildPerspectiveProjectionDegrees(float fovDegrees, float aspect, float nearPlane, float farPlane) {
    return math::Perspective(
        static_cast<float>(fovDegrees * 3.14159265f / 180.0f),
        aspect,
        nearPlane,
        farPlane);
}

inline math::Mat4 BuildOrthographicProjection(float orthoSize, float aspect, float nearPlane, float farPlane) {
    const float half = orthoSize * 0.5f;
    return math::Orthographic(-half * aspect, half * aspect, -half, half, nearPlane, farPlane);
}

inline math::Mat4 BuildViewFromWorldTransform(const math::Vec3& position, const math::Quat& rotation, const math::Vec3& scale) {
    const math::Mat4 world = math::Compose(position, rotation, scale);
    return math::Inverse(world);
}

inline math::Mat4 BuildLookAtView(const math::Vec3& eye, const math::Vec3& target, const math::Vec3& up) {
    return math::LookAt(eye, target, up);
}

inline CameraView BuildFreeFlyCameraView(const FreeFlyCameraDesc& cam, float viewportWidth, float viewportHeight) {
    CameraView out{};
    const float aspect = (viewportHeight > 0.0f) ? (viewportWidth / viewportHeight) : 1.0f;
    out.nearPlane = cam.nearPlane;
    out.farPlane = cam.farPlane;
    out.projection = BuildPerspectiveProjectionDegrees(cam.fovDegrees, aspect, out.nearPlane, out.farPlane);
    const math::Vec3 forward = ComputeForward(cam.yaw, cam.pitch);
    const math::Vec3 target = cam.position + forward;
    out.viewPos = cam.position;
    out.view = BuildLookAtView(cam.position, target, math::Vec3(0.0f, 1.0f, 0.0f));
    return out;
}

inline math::Mat4 AdjustProjectionForBackend(const math::Mat4& projection, const CameraPolicy& policy) {
    math::Mat4 fix(1.0f);

    if (policy.flipClipY) {
        fix[1][1] = -1.0f;
    }

    if (policy.depthRange == DepthRange::ZeroToOne) {
        fix[2][2] *= 0.5f;
        fix[3][2] += 0.5f;
    }

    (void)policy.reverseZ;

    return fix * projection;
}

inline CameraView AdjustCameraForBackend(const CameraView& inCamera, const CameraPolicy& policy) {
    CameraView out = inCamera;
    out.projection = AdjustProjectionForBackend(inCamera.projection, policy);
    return out;
}

inline CameraView AdjustCameraForBackend(const CameraView& inCamera, RenderBackend backend) {
    return AdjustCameraForBackend(inCamera, PolicyForBackend(backend));
}

}  // namespace camera
}  // namespace rendering
}  // namespace eyelib
