#pragma once

#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {
struct IDevice;
struct ICommandList;
struct IRenderTarget;
struct IBuffer;
}

/**
 * Owns long-lived frame resources (HDR RT, depth, etc.) — physical allocation (doc `core/RenderResources`).
 * Published onto the frame graph blackboard by `RenderWorld` each frame.
 */
class RenderResources {
public:
    static constexpr size_t kStagingUploadBytes = 256u * 1024u;
    /** Small device-local buffer for staging → GPU copies from `OnUpload` (e.g. frame constants). */
    static constexpr size_t kGpuUploadScratchBytes = 256u;

    RenderResources() = default;

    void EnsureAllocated(rhi::IDevice* device, uint32_t width, uint32_t height);
    void Shutdown(rhi::IDevice* device) noexcept;

    /** Host-visible upload ring (lazy). Safe for `OnUpload` writers. */
    void EnsureStagingUploadBuffer(rhi::IDevice* device);
    rhi::IBuffer* StagingUploadBuffer() noexcept { return m_staging; }
    const rhi::IBuffer* StagingUploadBuffer() const noexcept { return m_staging; }

    void EnsureGpuUploadScratchBuffer(rhi::IDevice* device);
    rhi::IBuffer* GpuUploadScratchBuffer() noexcept { return m_gpuScratch; }
    const rhi::IBuffer* GpuUploadScratchBuffer() const noexcept { return m_gpuScratch; }

    /**
     * CPU write to host staging, then record `CopyBuffer` into the device-local scratch UBO with correct barriers.
     * Owns buffer-state sequencing (no per-feature "primed" flags). Call only from `OnUpload` while `cmd` is recording.
     * @return false if `cmd`/buffers/`data` invalid or `sizeInBytes` > `kGpuUploadScratchBytes`.
     */
    bool RecordGpuUploadScratchFromStaging(rhi::ICommandList* cmd, const void* data, size_t sizeInBytes);

    uint32_t Width() const noexcept { return m_width; }
    uint32_t Height() const noexcept { return m_height; }

    /** Combined HDR + depth target; null until first successful `EnsureAllocated`. */
    rhi::IRenderTarget* MainHdrRenderTarget() noexcept { return m_mainHdr; }
    const rhi::IRenderTarget* MainHdrRenderTarget() const noexcept { return m_mainHdr; }

private:
    rhi::IDevice* m_device = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    rhi::IRenderTarget* m_mainHdr = nullptr;
    rhi::IBuffer* m_staging = nullptr;
    rhi::IBuffer* m_gpuScratch = nullptr;
    /** Staging was used as a transfer source on the GPU (needs transfer→host before next CPU write). */
    bool m_stagingGpuReadPending = false;
    /** Scratch buffer was transitioned to uniform read after last successful upload. */
    bool m_gpuScratchUniformReady = false;
};

}  // namespace rendering
}  // namespace eyelib
