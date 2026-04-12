#include "eyelib/rendering/visibility/VisibilitySystem.h"
#include "eyelib/rendering/scene/DrawList.h"
#include "eyelib/rendering/scene/IRenderSceneView.h"

namespace eyelib {
namespace rendering {

void VisibilitySystem::BuildCpuDrawList(const IRenderSceneView* scene, DrawList& out) const {
    out.Clear();
    if (!scene)
        return;
    if (scene->TryGetCamera() != nullptr) {
        out.placeholderIndices.push_back(1u);
        out.visibleCount = 1u;
    }
}

}  // namespace rendering
}  // namespace eyelib
