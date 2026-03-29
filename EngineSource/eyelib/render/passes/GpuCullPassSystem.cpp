#include "eyelib/render/passes/GpuCullPassSystem.h"
#include "eyelib/render/passes/GpuCullPass.h"
#include "eyelib/render/IRenderSceneView.h"
#include "eyelib/math/Types.h"

namespace eyelib {
namespace render {

GpuCullPassSystem::~GpuCullPassSystem() {
    Shutdown();
}

bool GpuCullPassSystem::Initialize(rhi::IDevice* device) {
    Shutdown();
    if (!device) return false;
    m_device = device;
    m_gpuCullPass = std::make_unique<GpuCullPass>();
    if (m_gpuCullPass->IsSupported(m_device))
        m_gpuCullPass->Initialize(m_device);
    if (m_gpuCullPass->IsSupported(m_device)) {
        rhi::BufferDesc bd;
        bd.sizeInBytes = kMaxGpuCullDrawEntries * sizeof(math::Mat4);
        bd.usage = rhi::BufferUsage::Storage;
        bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_transformBuffer = m_device->CreateBuffer(bd, nullptr);
    }
    return true;
}

void GpuCullPassSystem::Shutdown() {
    if (m_transformBuffer && m_device) {
        m_device->DestroyBuffer(m_transformBuffer);
        m_transformBuffer = nullptr;
    }
    m_gpuCullPass.reset();
    m_device = nullptr;
}

bool GpuCullPassSystem::IsSupported(rhi::IDevice* device) const {
    return m_gpuCullPass && m_gpuCullPass->IsSupported(device);
}

uint32_t GpuCullPassSystem::PrepareFromVisibleOpaque(const GpuCullScenePrepUploadParams& upload,
                                                     const std::vector<Drawable>& visibleOpaque) {
    return RunGpuCullScenePrepFromDrawables(m_scratch, upload, visibleOpaque);
}

uint32_t GpuCullPassSystem::PrepareFromView(const GpuCullScenePrepUploadParams& upload, const IRenderSceneView& view) {
    return RunGpuCullScenePrepFromView(m_scratch, upload, view);
}

}  // namespace render
}  // namespace eyelib
