#pragma once

#include "eyelib/rendering/core/FrameViewUniforms.h"

#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {
struct IDevice;
struct ICommandList;
struct IRenderTarget;
struct ITexture;
struct IBuffer;
}

/**
 * Owns long-lived frame resources (HDR color RT, depth texture, PerFrame/PerView UBOs, upload scaffolding).
 * Published onto the frame graph blackboard by `RenderWorld` each frame (doc `core/RenderResources`).
 *
 * **Upload contract (A.8):** `EnsureAllocated` / `EnsureUniformBuffers` / `Ensure*Buffer` are allocation/setup only
 * (no upload commands); `RenderWorld` runs them in a **prepare** step before `BeginUploadRecordingPhase`.
 * `UploadPer*` / `RecordGpuUploadScratchFromStaging` record GPU copies on the upload command list and assert
 * (Debug) that they run only while the upload recording scope is active (`OnUpload`).
 *
 * Depth split: `m_mainDepth` is a standalone depth texture; `m_mainHdr` is an HDR color RT that
 * binds this depth via `CreateRenderTargetWithSharedDepth`, so both slots reference the same
 * physical attachment while being independently addressable on the blackboard.
 */
class RenderResources {
public:
    static constexpr size_t kStagingUploadBytes = 256u * 1024u;
    /** Small device-local buffer for staging → GPU copies from `OnUpload` (e.g. frame constants). */
    static constexpr size_t kGpuUploadScratchBytes = 256u;

    RenderResources() = default;

    void EnsureAllocated(rhi::IDevice* device, uint32_t width, uint32_t height);
    void Shutdown(rhi::IDevice* device) noexcept;

    /**
     * Bracket `IRenderFeature::OnUpload` (A.8). Debug: `UploadPerFrameUniform` / `UploadPerViewUniform` /
     * `RecordGpuUploadScratchFromStaging` assert when the depth is zero. Release: no-op.
     * `RenderWorld::UploadPhase` calls `Begin` immediately before the OnUpload loop and `End` after.
     */
    void BeginUploadRecordingPhase() noexcept;
    void EndUploadRecordingPhase() noexcept;

    /** Creates PerFrame / PerView uniform buffers if missing (device-local, transfer + uniform). */
    void EnsureUniformBuffers(rhi::IDevice* device);

    /** Host-visible upload ring (lazy). Safe for `OnUpload` writers. */
    void EnsureStagingUploadBuffer(rhi::IDevice* device);
    rhi::IBuffer* StagingUploadBuffer() noexcept { return m_staging; }
    const rhi::IBuffer* StagingUploadBuffer() const noexcept { return m_staging; }

    void EnsureGpuUploadScratchBuffer(rhi::IDevice* device);
    rhi::IBuffer* GpuUploadScratchBuffer() noexcept { return m_gpuScratch; }
    const rhi::IBuffer* GpuUploadScratchBuffer() const noexcept { return m_gpuScratch; }

    /**
     * CPU write to host staging, then record `CopyBuffer` into the device-local scratch UBO with correct barriers.
     * Owns buffer-state sequencing (no per-feature "primed" flags). Debug: must run inside `BeginUploadRecordingPhase`
     * (see `RenderWorld::UploadPhase` / `OnUpload`). @return false if `cmd`/buffers/`data` invalid or `sizeInBytes` > `kGpuUploadScratchBytes`.
     */
    bool RecordGpuUploadScratchFromStaging(rhi::ICommandList* cmd, const void* data, size_t sizeInBytes);

    /** Upload std140 PerFrame block into `PerFrameUniformBuffer` (OnUpload only). */
    bool UploadPerFrameUniform(rhi::ICommandList* cmd, const PerFrameUniformStd140& data);
    /** Upload std140 PerView block into `PerViewUniformBuffer` (OnUpload only). */
    bool UploadPerViewUniform(rhi::ICommandList* cmd, const PerViewUniformStd140& data);

    uint32_t Width() const noexcept { return m_width; }
    uint32_t Height() const noexcept { return m_height; }

    /** HDR color + shared depth render target; null until first successful `EnsureAllocated`. */
    rhi::IRenderTarget* MainHdrRenderTarget() noexcept { return m_mainHdr; }
    const rhi::IRenderTarget* MainHdrRenderTarget() const noexcept { return m_mainHdr; }

    /** Standalone depth texture attached to `MainHdrRenderTarget`; also published on its own blackboard slot. */
    rhi::ITexture* MainDepthTexture() noexcept { return m_mainDepth; }
    const rhi::ITexture* MainDepthTexture() const noexcept { return m_mainDepth; }

    /** PerFrame UBO (time, frame index). Created by `EnsureUniformBuffers`. */
    rhi::IBuffer* PerFrameUniformBuffer() noexcept { return m_perFrameUbo; }
    const rhi::IBuffer* PerFrameUniformBuffer() const noexcept { return m_perFrameUbo; }

    /** PerView UBO (camera, viewport). Created by `EnsureUniformBuffers`. */
    rhi::IBuffer* PerViewUniformBuffer() noexcept { return m_perViewUbo; }
    const rhi::IBuffer* PerViewUniformBuffer() const noexcept { return m_perViewUbo; }

private:
    bool RecordUniformBufferUploadFromStaging(rhi::ICommandList* cmd, rhi::IBuffer* dst, bool& uniformPrimed, const void* data,
                                            size_t sizeInBytes);

    rhi::IDevice* m_device = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    rhi::IRenderTarget* m_mainHdr = nullptr;
    rhi::ITexture* m_mainDepth = nullptr;
    rhi::IBuffer* m_staging = nullptr;
    rhi::IBuffer* m_gpuScratch = nullptr;
    rhi::IBuffer* m_perFrameUbo = nullptr;
    rhi::IBuffer* m_perViewUbo = nullptr;
    bool m_perFrameUboUniformPrimed = false;
    bool m_perViewUboUniformPrimed = false;
    /** Staging was used as a transfer source on the GPU (needs transfer→host before next CPU write). */
    bool m_stagingGpuReadPending = false;
    /** Scratch buffer was transitioned to uniform read after last successful upload. */
    bool m_gpuScratchUniformReady = false;
#if !defined(NDEBUG)
    /** Nested `BeginUploadRecordingPhase` count; GPU upload APIs require depth > 0. */
    uint32_t m_debugUploadRecordingDepth = 0;
#endif
};

}  // namespace rendering
}  // namespace eyelib
