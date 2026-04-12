#include "eyelib/rendering/core/RenderResources.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

#include <vector>

namespace eyelib {
namespace rendering {

void RenderResources::Shutdown(rhi::IDevice* device) noexcept {
    if (m_gpuScratch && device) {
        device->DestroyBuffer(m_gpuScratch);
        m_gpuScratch = nullptr;
    }
    if (m_staging && device) {
        device->DestroyBuffer(m_staging);
        m_staging = nullptr;
    }
    if (m_mainHdr && device)
        device->DestroyRenderTarget(m_mainHdr);
    m_mainHdr = nullptr;
    m_width = 0;
    m_height = 0;
    m_device = nullptr;
    m_stagingGpuReadPending = false;
    m_gpuScratchUniformReady = false;
}

void RenderResources::EnsureStagingUploadBuffer(rhi::IDevice* device) {
    if (!device || m_staging)
        return;
    rhi::BufferDesc desc{};
    desc.sizeInBytes = kStagingUploadBytes;
    desc.usage = rhi::BufferUsage::TransferSrc;
    desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
    desc.debugName = "EyelibRendering.UploadStaging";
    m_staging = device->CreateBuffer(desc, nullptr);
}

void RenderResources::EnsureGpuUploadScratchBuffer(rhi::IDevice* device) {
    if (!device || m_gpuScratch)
        return;
    rhi::BufferDesc desc{};
    desc.sizeInBytes = kGpuUploadScratchBytes;
    desc.usage = rhi::BufferUsage::TransferDst | rhi::BufferUsage::Uniform;
    desc.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
    desc.debugName = "EyelibRendering.GpuUploadScratch";
    m_gpuScratch = device->CreateBuffer(desc, nullptr);
}

void RenderResources::EnsureAllocated(rhi::IDevice* device, uint32_t width, uint32_t height) {
    if (!device || width == 0 || height == 0)
        return;
    if (m_mainHdr && m_device == device && m_width == width && m_height == height)
        return;

    Shutdown(device);
    m_device = device;
    m_width = width;
    m_height = height;

    std::vector<rhi::RHITextureFormat> colorFormats;
    colorFormats.push_back(rhi::RHITextureFormat::RGBA16F);
    m_mainHdr = device->CreateRenderTarget(width, height, colorFormats, rhi::RHITextureFormat::Depth24Stencil8, 1, false);
}

bool RenderResources::RecordGpuUploadScratchFromStaging(rhi::ICommandList* cmd, const void* data, size_t sizeInBytes) {
    if (!cmd || !data || sizeInBytes == 0 || sizeInBytes > kGpuUploadScratchBytes)
        return false;
    if (!m_staging || !m_gpuScratch)
        return false;

    if (m_stagingGpuReadPending)
        cmd->BarrierBufferTransferReadToHostWrite(m_staging);

    m_staging->Update(data, 0, sizeInBytes);
    cmd->BarrierHostVisibleBufferToTransferRead(m_staging);

    if (m_gpuScratchUniformReady)
        cmd->BarrierBuffer(m_gpuScratch, rhi::ResourceState::UniformBuffer, rhi::ResourceState::TransferDst);
    else
        cmd->BarrierBuffer(m_gpuScratch, rhi::ResourceState::Undefined, rhi::ResourceState::TransferDst);

    cmd->CopyBuffer(m_staging, 0, m_gpuScratch, 0, sizeInBytes);
    cmd->BarrierBuffer(m_gpuScratch, rhi::ResourceState::TransferDst, rhi::ResourceState::UniformBuffer);

    m_stagingGpuReadPending = true;
    m_gpuScratchUniformReady = true;
    return true;
}

}  // namespace rendering
}  // namespace eyelib
