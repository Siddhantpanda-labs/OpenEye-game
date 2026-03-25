#pragma once

#include "api/render/RenderScene.h"
#include "api/render/RenderBackend.h"
#include "eyelib/render/CameraConventions.h"

#include <cstring>

namespace eyelib::api::camera {
using FreeFlyCameraDesc = ::eyelib::render::camera::FreeFlyCameraDesc;
using ::eyelib::render::camera::ComputeForward;
using ::eyelib::render::camera::DepthRange;
using ::eyelib::render::camera::CameraPolicy;
using ::eyelib::render::camera::PolicyForBackend;
using ::eyelib::render::camera::AdjustProjectionForBackend;
using ::eyelib::render::camera::BuildPerspectiveProjectionDegrees;
using ::eyelib::render::camera::BuildOrthographicProjection;
using ::eyelib::render::camera::BuildLookAtView;
using ::eyelib::render::camera::BuildViewFromWorldTransform;

inline CameraView BuildFreeFlyCameraView(const FreeFlyCameraDesc& cam, float viewportWidth, float viewportHeight) {
    const auto libView = ::eyelib::render::camera::BuildFreeFlyCameraView(cam, viewportWidth, viewportHeight);
    CameraView out{};
    out.view = libView.view;
    out.projection = libView.projection;
    out.viewPos = libView.viewPos;
    out.nearPlane = libView.nearPlane;
    out.farPlane = libView.farPlane;
    return out;
}

/** Typed backend first so legacy string overload can delegate without forward-declare issues. */
inline CameraView AdjustCameraForBackend(const CameraView& inCamera, ::eyelib::render::RenderBackend backend) {
    ::eyelib::render::CameraView libIn{};
    libIn.view = inCamera.view;
    libIn.projection = inCamera.projection;
    libIn.viewPos = inCamera.viewPos;
    libIn.nearPlane = inCamera.nearPlane;
    libIn.farPlane = inCamera.farPlane;
    const auto libOut = ::eyelib::render::camera::AdjustCameraForBackend(libIn, backend);
    CameraView out{};
    out.view = libOut.view;
    out.projection = libOut.projection;
    out.viewPos = libOut.viewPos;
    out.nearPlane = libOut.nearPlane;
    out.farPlane = libOut.farPlane;
    return out;
}

inline CameraView AdjustCameraForBackend(const CameraView& inCamera, const char* backendName) {
    ::eyelib::render::RenderBackend backend = ::eyelib::render::RenderBackend::Default;
    if (backendName) {
        if (std::strcmp(backendName, "vulkan") == 0) backend = ::eyelib::render::RenderBackend::Vulkan;
        else if (std::strcmp(backendName, "opengl") == 0) backend = ::eyelib::render::RenderBackend::OpenGL;
        else if (std::strcmp(backendName, "diligent") == 0) backend = ::eyelib::render::RenderBackend::Diligent;
    }
    return AdjustCameraForBackend(inCamera, backend);
}
} // namespace eyelib::api::camera
