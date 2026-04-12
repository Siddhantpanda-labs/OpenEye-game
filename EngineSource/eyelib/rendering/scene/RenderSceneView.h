#pragma once

#include "eyelib/rendering/scene/CameraView.h"
#include "eyelib/rendering/scene/IRenderSceneView.h"

namespace eyelib {
namespace rendering {

/** Default concrete scene view (camera only for now). */
class RenderSceneView final : public IRenderSceneView {
public:
    CameraView camera{};

    const CameraView* TryGetCamera() const override { return &camera; }
};

}  // namespace rendering
}  // namespace eyelib
