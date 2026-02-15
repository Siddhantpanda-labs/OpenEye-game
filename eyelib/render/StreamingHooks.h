#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstddef>
#include <functional>

namespace eyelib {
namespace render {

/**
 * E7 / LEVEL 8: Architecture hooks for texture streaming, residency, async upload, and budget.
 * Default implementations are no-op or return safe defaults; the engine can override via setters.
 */

struct StreamingBudget {
    size_t budgetBytes = 0;       ///< Max bytes for streaming pool (0 = no limit / unknown)
    size_t currentUsageBytes = 0; ///< Estimated current usage
};

/** Called when an async buffer upload completes. */
using AsyncUploadCallback = std::function<void()>;

/** Return current streaming budget (default: 0, 0). */
StreamingBudget GetStreamingBudget();
/** Provide a custom budget getter (e.g. from GPU memory query). */
void SetStreamingBudgetGetter(std::function<StreamingBudget()> getter);

/** True if the texture is resident and safe to use (default: true for all). */
bool IsResourceResident(rhi::ITexture* texture);
/** Mark texture resident/evicted (default: no-op). */
void SetResourceResident(rhi::ITexture* texture, bool resident);
/** Provide custom residency query/set (e.g. for virtual textures). */
void SetResidencyHooks(std::function<bool(rhi::ITexture*)> query, std::function<void(rhi::ITexture*, bool)> set);

/** Request streaming of a mip level or quality tier (default: no-op). */
void RequestTextureStream(rhi::ITexture* texture, uint32_t mipLevelOrQuality);
/** Provide custom texture stream request handler. */
void SetTextureStreamRequest(std::function<void(rhi::ITexture*, uint32_t)> handler);

/** Opaque mesh/LOD handle; engine interprets. Request load of a specific LOD (default: no-op). */
void RequestMeshLOD(void* meshHandle, uint32_t lodLevel);
void SetMeshLODRequest(std::function<void(void* meshHandle, uint32_t lodLevel)> handler);

/**
 * Request async upload of buffer data. Default: sync upload (buffer->Update) then callback.
 * Override to use async DMA/transfer queue.
 */
void RequestAsyncBufferUpload(rhi::IDevice* device, rhi::IBuffer* buffer,
                              size_t offset, size_t size, const void* data,
                              AsyncUploadCallback onComplete);
/** Provide custom async upload implementation. */
void SetAsyncBufferUpload(std::function<void(rhi::IDevice*, rhi::IBuffer*, size_t, size_t, const void*, AsyncUploadCallback)> impl);

} // namespace render
} // namespace eyelib
