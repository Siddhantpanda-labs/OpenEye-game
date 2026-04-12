#pragma once

namespace eyelib {
namespace rendering {

class IRenderSceneView;
struct DrawList;

/** CPU-side visibility; produces `DrawList` for downstream geometry / passes (doc `visibility/`). */
class VisibilitySystem {
public:
    VisibilitySystem() = default;

    /** Fills `out` from the scene view (camera-only stub until culling lands). */
    void BuildCpuDrawList(const IRenderSceneView* scene, DrawList& out) const;
};

}  // namespace rendering
}  // namespace eyelib
