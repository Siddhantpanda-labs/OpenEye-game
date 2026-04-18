#pragma once

#include "eyelib/rendering/scene/CameraView.h"
#include "eyelib/rendering/scene/IRenderSceneView.h"
#include "eyelib/rendering/scene/SceneDrawInstance.h"

#include <vector>

namespace eyelib {
namespace rendering {

/** Default concrete scene view: camera + optional drawable instances for CPU cull. */
class RenderSceneView final : public IRenderSceneView {
public:
    CameraView camera{};
    std::vector<SceneDrawInstance> drawInstances;

    const CameraView* TryGetCamera() const override { return &camera; }

    size_t SceneDrawInstanceCount() const override { return drawInstances.size(); }
    const SceneDrawInstance* SceneDrawInstances() const override {
        return drawInstances.empty() ? nullptr : drawInstances.data();
    }
};

}  // namespace rendering
}  // namespace eyelib
