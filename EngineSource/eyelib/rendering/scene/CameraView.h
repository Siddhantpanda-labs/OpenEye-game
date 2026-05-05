#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace rendering {

/** Camera view: view/projection matrices and view position (scene / frame contract). */
struct CameraView {
    math::Mat4 view{1.0f};
    math::Mat4 projection{1.0f};
    math::Vec3 viewPos{0, 0, 0};
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

}  // namespace rendering
}  // namespace eyelib
