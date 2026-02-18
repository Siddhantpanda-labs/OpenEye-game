#include "eyelib/render/rhi/OpenGL/PipelineGL.h"

namespace eyelib {
namespace render {
namespace rhi {

// State is applied at bind time by CommandListGL; no GL calls here.

void PipelineGL::GetComputeBlockPresence(ComputeBlockPresence& out) const {
    out = m_computeBlockPresence;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
