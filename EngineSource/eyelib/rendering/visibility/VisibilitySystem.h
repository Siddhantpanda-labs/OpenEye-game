#pragma once

namespace eyelib {
namespace rendering {

class IRenderSceneView;
struct DrawList;

/** CPU-side visibility; produces `DrawList` for downstream geometry / passes (doc `visibility/`). */
class VisibilitySystem {
public:
    VisibilitySystem() = default;

    /** Frustum-culls `scene` draw instances into `out`, assigns `DrawItem::sortKey`, stable sort ascending. */
    void BuildCpuDrawList(const IRenderSceneView* scene, DrawList& out) const;
};

}  // namespace rendering
}  // namespace eyelib
