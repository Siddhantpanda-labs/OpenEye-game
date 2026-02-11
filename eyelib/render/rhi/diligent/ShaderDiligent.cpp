#include "eyelib/render/rhi/diligent/ShaderDiligent.h"
#if OPENEYE_USE_DILIGENT
#include "Graphics/GraphicsEngine/interface/Shader.h"
#endif

namespace eyelib {
namespace render {
namespace rhi {

ShaderDiligent::~ShaderDiligent() {
#if OPENEYE_USE_DILIGENT
    if (m_pNative) {
        static_cast<Diligent::IShader*>(m_pNative)->Release();
        m_pNative = nullptr;
    }
#endif
}

ShaderStage ShaderDiligent::GetStage() const { return m_stage; }
bool ShaderDiligent::IsValid() const { return m_valid; }

} // namespace rhi
} // namespace render
} // namespace eyelib
