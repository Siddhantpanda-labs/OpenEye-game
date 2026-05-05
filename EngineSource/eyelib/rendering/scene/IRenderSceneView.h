#pragma once

#include <cstddef>

namespace eyelib {
namespace rendering {

struct CameraView;
struct SceneDrawInstance;

/**
 * Read-only scene contract for the renderer (GPU-agnostic).
 * Extended incrementally (lights, draw lists, …).
 */
class IRenderSceneView {
public:
    virtual ~IRenderSceneView() = default;

    /** Main camera for this view; null when not provided. */
    virtual const CameraView* TryGetCamera() const { return nullptr; }

    /** Instances to consider for CPU visibility (may be empty). Default: none. */
    virtual size_t SceneDrawInstanceCount() const { return 0; }
    virtual const SceneDrawInstance* SceneDrawInstances() const { return nullptr; }
};

}  // namespace rendering
}  // namespace eyelib
