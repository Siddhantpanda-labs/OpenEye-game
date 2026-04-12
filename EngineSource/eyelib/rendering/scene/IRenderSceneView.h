#pragma once

namespace eyelib {
namespace rendering {

struct CameraView;

/**
 * Read-only scene contract for the renderer (GPU-agnostic).
 * Extended incrementally (lights, draw lists, …).
 */
class IRenderSceneView {
public:
    virtual ~IRenderSceneView() = default;

    /** Main camera for this view; null when not provided. */
    virtual const CameraView* TryGetCamera() const { return nullptr; }
};

}  // namespace rendering
}  // namespace eyelib
