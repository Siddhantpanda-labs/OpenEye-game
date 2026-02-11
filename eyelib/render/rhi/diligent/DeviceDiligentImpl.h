#pragma once

#if OPENEYE_USE_DILIGENT
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Common/interface/RefCntAutoPtr.hpp"

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceDiligentImpl {
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>   pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext>   pContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>      pSwapChain;
};

} // namespace rhi
} // namespace render
} // namespace eyelib

#else

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceDiligentImpl {};

} // namespace rhi
} // namespace render
} // namespace eyelib

#endif
