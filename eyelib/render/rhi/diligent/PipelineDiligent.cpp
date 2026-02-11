#include "eyelib/render/rhi/diligent/PipelineDiligent.h"
#if OPENEYE_USE_DILIGENT
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#endif

namespace eyelib {
namespace render {
namespace rhi {

PipelineDiligent::~PipelineDiligent() {
#if OPENEYE_USE_DILIGENT
    if (m_pSRB) {
        static_cast<Diligent::IShaderResourceBinding*>(m_pSRB)->Release();
        m_pSRB = nullptr;
    }
    if (m_pNative) {
        static_cast<Diligent::IPipelineState*>(m_pNative)->Release();
        m_pNative = nullptr;
    }
#endif
}

} // namespace rhi
} // namespace render
} // namespace eyelib
