#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Camera view: view/projection matrices, world-space view position, near/far planes.
 *
 * POD; safe to copy. Filled by editor/player on the game thread, consumed by
 * `RenderPipeline::Render(RenderScene&)` on the render thread (or same thread when there is
 * no render thread). Convention follows the new module's `eyelib::rendering::CameraView`
 * (right-handed, -Z forward), but is declared independently here so that no
 * `eyelib::rendering::*` types leak into the public API surface.
 *
 * Mirrors legacy `eyelib::api::CameraView` field-for-field for migration parity.
 */
struct CameraView {
    math::Mat4 view{1.0f};
    math::Mat4 projection{1.0f};
    math::Vec3 viewPos{0.0f, 0.0f, 0.0f};
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
