#include "eyelib/rendering/core/RenderResources.h"
#include "eyelib/rendering/core/PerObjectUniforms.h"
#include "eyelib/core/Log.h"
#include "eyelib/rendering/scene/DrawList.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace eyelib {
namespace rendering {

void RenderResources::BeginUploadRecordingPhase() noexcept {
#if !defined(NDEBUG)
    ++m_debugUploadRecordingDepth;
#endif
}

void RenderResources::EndUploadRecordingPhase() noexcept {
#if !defined(NDEBUG)
    assert(m_debugUploadRecordingDepth > 0 && "RenderResources::EndUploadRecordingPhase without matching Begin");
    --m_debugUploadRecordingDepth;
#endif
}

void RenderResources::Shutdown(rhi::IDevice* device) noexcept {
#if !defined(NDEBUG)
    assert(m_debugUploadRecordingDepth == 0 && "RenderResources::Shutdown while upload recording scope is active");
#endif
    if (m_perViewUbo && device) {
        device->DestroyBuffer(m_perViewUbo);
        m_perViewUbo = nullptr;
    }
    if (m_perObjectSsbo && device) {
        device->DestroyBuffer(m_perObjectSsbo);
        m_perObjectSsbo = nullptr;
    }
    m_perObjectCapacity = 0;
    m_perObjectSsboStoragePrimed = false;
    if (m_dirLightUbo && device) {
        device->DestroyBuffer(m_dirLightUbo);
        m_dirLightUbo = nullptr;
    }
    if (m_perFrameUbo && device) {
        device->DestroyBuffer(m_perFrameUbo);
        m_perFrameUbo = nullptr;
    }
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
    if (m_mainDepth && device)
        device->DestroyTexture(m_mainDepth);
    m_mainDepth = nullptr;
    m_width = 0;
    m_height = 0;
    m_device = nullptr;
    m_stagingGpuReadPending = false;
    m_gpuScratchUniformReady = false;
    m_perFrameUboUniformPrimed = false;
    m_perViewUboUniformPrimed = false;
    m_dirLightUboUniformPrimed = false;
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

void RenderResources::EnsurePerObjectBuffer(rhi::IDevice* device, uint32_t minInstanceCount) {
    if (!device)
        return;
    const uint32_t need = (std::max)(minInstanceCount, 1u);
    uint32_t cap = 64u;
    while (cap < need) {
        if (cap > UINT32_MAX / 2u)
            break;
        cap *= 2u;
    }
    if (m_perObjectSsbo && m_perObjectCapacity >= cap)
        return;

    if (m_perObjectSsbo && device)
        device->DestroyBuffer(m_perObjectSsbo);
    m_perObjectSsbo = nullptr;
    m_perObjectCapacity = 0;
    m_perObjectSsboStoragePrimed = false;

    rhi::BufferDesc d{};
    d.sizeInBytes = static_cast<size_t>(cap) * sizeof(PerObjectGpuStd430);
    d.usage = rhi::BufferUsage::TransferDst | rhi::BufferUsage::Storage;
    d.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
    d.debugName = "EyelibRendering.PerObjectSsbo";
    m_perObjectSsbo = device->CreateBuffer(d, nullptr);
    if (m_perObjectSsbo)
        m_perObjectCapacity = cap;
}

void RenderResources::EnsureUniformBuffers(rhi::IDevice* device) {
    if (!device)
        return;
    if (!m_perFrameUbo) {
        rhi::BufferDesc d{};
        d.sizeInBytes = kPerFrameUniformGpuBytes;
        d.usage = rhi::BufferUsage::TransferDst | rhi::BufferUsage::Uniform;
        d.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
        d.debugName = "EyelibRendering.PerFrameUbo";
        m_perFrameUbo = device->CreateBuffer(d, nullptr);
        m_perFrameUboUniformPrimed = false;
    }
    if (!m_perViewUbo) {
        rhi::BufferDesc d{};
        d.sizeInBytes = kPerViewUniformGpuBytes;
        d.usage = rhi::BufferUsage::TransferDst | rhi::BufferUsage::Uniform;
        d.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
        d.debugName = "EyelibRendering.PerViewUbo";
        m_perViewUbo = device->CreateBuffer(d, nullptr);
        m_perViewUboUniformPrimed = false;
    }
    if (!m_dirLightUbo) {
        rhi::BufferDesc d{};
        d.sizeInBytes = kDirectionalLightUniformGpuBytes;
        d.usage = rhi::BufferUsage::TransferDst | rhi::BufferUsage::Uniform;
        d.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
        d.debugName = "EyelibRendering.DirectionalLightUbo";
        m_dirLightUbo = device->CreateBuffer(d, nullptr);
        m_dirLightUboUniformPrimed = false;
    }
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
    if (m_mainHdr && m_mainDepth && m_device == device && m_width == width && m_height == height)
        return;

    Shutdown(device);
    m_device = device;
    m_width = width;
    m_height = height;

    rhi::TextureDesc depthDesc{};
    depthDesc.type = rhi::TextureType::Texture2D;
    depthDesc.width = width;
    depthDesc.height = height;
    depthDesc.format = rhi::RHITextureFormat::Depth24Stencil8;
    depthDesc.usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::ShaderResource |
                      rhi::TextureUsage::TransferSrc | rhi::TextureUsage::TransferDst;
    depthDesc.mipLevels = 1;
    depthDesc.samples = 1;
    depthDesc.debugName = "EyelibRendering.MainDepth";
    m_mainDepth = device->CreateTexture(depthDesc, nullptr);
    if (!m_mainDepth)
        return;

    std::vector<rhi::RHITextureFormat> colorFormats;
    colorFormats.push_back(rhi::RHITextureFormat::RGBA16F);
    m_mainHdr = device->CreateRenderTargetWithSharedDepth(width, height, colorFormats, m_mainDepth, 1);
}

bool RenderResources::RecordUniformBufferUploadFromStaging(rhi::ICommandList* cmd, rhi::IBuffer* dst, bool& uniformPrimed, const void* data,
                                                          size_t sizeInBytes) {
#if !defined(NDEBUG)
    assert(m_debugUploadRecordingDepth > 0 &&
           "RenderResources: buffer upload recording only allowed during upload phase (RenderWorld brackets OnUpload with Begin/End).");
#endif
    if (!cmd || !dst || !data || sizeInBytes == 0 || sizeInBytes > kStagingUploadBytes) {
        static int s_invalidArgsWarnCount = 0;
        if (s_invalidArgsWarnCount < 8) {
            ++s_invalidArgsWarnCount;
            core::Log::WarnF("RenderResources::RecordUniformBufferUploadFromStaging invalid args: cmd=%s dst=%s data=%s size=%zu max=%zu",
                             cmd ? "ok" : "null",
                             dst ? "ok" : "null",
                             data ? "ok" : "null",
                             sizeInBytes,
                             kStagingUploadBytes);
        }
        return false;
    }
    if (!m_staging) {
        static int s_missingStagingWarnCount = 0;
        if (s_missingStagingWarnCount < 8) {
            ++s_missingStagingWarnCount;
            core::Log::Warn("RenderResources::RecordUniformBufferUploadFromStaging missing staging upload buffer.");
        }
        return false;
    }

    if (m_stagingGpuReadPending)
        cmd->BarrierBufferTransferReadToHostWrite(m_staging);

    m_staging->Update(data, 0, sizeInBytes);
    cmd->BarrierHostVisibleBufferToTransferRead(m_staging);

    if (uniformPrimed)
        cmd->BarrierBuffer(dst, rhi::ResourceState::UniformBuffer, rhi::ResourceState::TransferDst);
    else
        cmd->BarrierBuffer(dst, rhi::ResourceState::Undefined, rhi::ResourceState::TransferDst);

    cmd->CopyBuffer(m_staging, 0, dst, 0, sizeInBytes);
    cmd->BarrierBuffer(dst, rhi::ResourceState::TransferDst, rhi::ResourceState::UniformBuffer);

    m_stagingGpuReadPending = true;
    uniformPrimed = true;
    return true;
}

bool RenderResources::UploadPerFrameUniform(rhi::ICommandList* cmd, const PerFrameUniformStd140& data) {
    if (!m_perFrameUbo)
        return false;
    return RecordUniformBufferUploadFromStaging(cmd, m_perFrameUbo, m_perFrameUboUniformPrimed, &data, sizeof(data));
}

bool RenderResources::UploadPerViewUniform(rhi::ICommandList* cmd, const PerViewUniformStd140& data) {
    if (!m_perViewUbo) {
        static int s_missingPerViewUboWarnCount = 0;
        if (s_missingPerViewUboWarnCount < 8) {
            ++s_missingPerViewUboWarnCount;
            core::Log::Warn("RenderResources::UploadPerViewUniform failed: m_perViewUbo is null.");
        }
        return false;
    }
    const bool ok = RecordUniformBufferUploadFromStaging(cmd, m_perViewUbo, m_perViewUboUniformPrimed, &data, sizeof(data));
    if (!ok) {
        static int s_uploadPerViewFailedWarnCount = 0;
        if (s_uploadPerViewFailedWarnCount < 8) {
            ++s_uploadPerViewFailedWarnCount;
            core::Log::WarnF(
                "RenderResources::UploadPerViewUniform failed: cmd=%s perViewUbo=%s staging=%s primed=%s",
                cmd ? "ok" : "null",
                m_perViewUbo ? "ok" : "null",
                m_staging ? "ok" : "null",
                m_perViewUboUniformPrimed ? "true" : "false");
        }
    }
    return ok;
}

bool RenderResources::UploadDirectionalLightUniform(rhi::ICommandList* cmd, const DirectionalLightStd140& data) {
    if (!m_dirLightUbo)
        return false;
    return RecordUniformBufferUploadFromStaging(cmd, m_dirLightUbo, m_dirLightUboUniformPrimed, &data, sizeof(data));
}

bool RenderResources::RecordStorageBufferUploadFromStaging(rhi::ICommandList* cmd, rhi::IBuffer* dst, bool& storagePrimed, const void* data,
                                                           size_t sizeInBytes) {
#if !defined(NDEBUG)
    assert(m_debugUploadRecordingDepth > 0 &&
           "RenderResources: buffer upload recording only allowed during upload phase (RenderWorld brackets OnUpload with Begin/End).");
#endif
    if (!cmd || !dst || !data || sizeInBytes == 0 || sizeInBytes > kStagingUploadBytes)
        return false;
    if (!m_staging)
        return false;

    if (m_stagingGpuReadPending)
        cmd->BarrierBufferTransferReadToHostWrite(m_staging);

    m_staging->Update(data, 0, sizeInBytes);
    cmd->BarrierHostVisibleBufferToTransferRead(m_staging);

    if (storagePrimed)
        cmd->BarrierBuffer(dst, rhi::ResourceState::StorageBuffer, rhi::ResourceState::TransferDst);
    else
        cmd->BarrierBuffer(dst, rhi::ResourceState::Undefined, rhi::ResourceState::TransferDst);

    cmd->CopyBuffer(m_staging, 0, dst, 0, sizeInBytes);
    cmd->BarrierBuffer(dst, rhi::ResourceState::TransferDst, rhi::ResourceState::StorageBuffer);

    m_stagingGpuReadPending = true;
    storagePrimed = true;
    return true;
}

bool RenderResources::UploadPerObjectBufferFromDrawList(rhi::ICommandList* cmd, const DrawList& list) {
    if (!m_perObjectSsbo)
        return false;
    const uint32_t n = list.VisibleCount();
    if (n == 0)
        return true;
    if (n > m_perObjectCapacity)
        return false;
    const size_t bytes = static_cast<size_t>(n) * sizeof(PerObjectGpuStd430);
    if (bytes > kStagingUploadBytes)
        return false;

    std::vector<PerObjectGpuStd430> rows(n);
    for (uint32_t i = 0; i < n; ++i) {
        const DrawItem& it = list.items[i];
        FillPerObjectGpuRow(it.worldMatrix, it.materialId, it.meshHandle, it.transformIndex, rows[i]);
    }
    if (n > 0) {
        static bool s_loggedFirstPerObjectRow = false;
        if (!s_loggedFirstPerObjectRow) {
            s_loggedFirstPerObjectRow = true;
            const PerObjectGpuStd430& r0 = rows[0];
            core::Log::InfoF(
                "RenderResources::UploadPerObjectBufferFromDrawList row0: materialId=%u meshHandle=%u transformIndex=%u "
                "modelCol0=(%.3f,%.3f,%.3f,%.3f) modelCol3=(%.3f,%.3f,%.3f,%.3f)",
                r0.materialId,
                r0.meshHandle,
                r0.transformIndex,
                r0.model[0], r0.model[1], r0.model[2], r0.model[3],
                r0.model[12], r0.model[13], r0.model[14], r0.model[15]);
        }
    }
    return RecordStorageBufferUploadFromStaging(cmd, m_perObjectSsbo, m_perObjectSsboStoragePrimed, rows.data(), bytes);
}

bool RenderResources::RecordGpuUploadScratchFromStaging(rhi::ICommandList* cmd, const void* data, size_t sizeInBytes) {
#if !defined(NDEBUG)
    assert(m_debugUploadRecordingDepth > 0 &&
           "RenderResources: GPU upload scratch recording only allowed during upload phase (OnUpload).");
#endif
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
