#include "eyelib/render/StreamingHooks.h"
#include <cstring>

namespace eyelib {
namespace render {

namespace {

std::function<StreamingBudget()> s_budgetGetter;
std::function<bool(rhi::ITexture*)> s_residencyQuery;
std::function<void(rhi::ITexture*, bool)> s_residencySet;
std::function<void(rhi::ITexture*, uint32_t)> s_textureStreamRequest;
std::function<void(void*, uint32_t)> s_meshLODRequest;
std::function<void(rhi::IDevice*, rhi::IBuffer*, size_t, size_t, const void*, AsyncUploadCallback)> s_asyncUpload;

} // namespace

StreamingBudget GetStreamingBudget() {
    if (s_budgetGetter)
        return s_budgetGetter();
    return {};
}

void SetStreamingBudgetGetter(std::function<StreamingBudget()> getter) {
    s_budgetGetter = std::move(getter);
}

bool IsResourceResident(rhi::ITexture* texture) {
    if (s_residencyQuery && texture)
        return s_residencyQuery(texture);
    return true;
}

void SetResourceResident(rhi::ITexture* texture, bool resident) {
    if (s_residencySet && texture)
        s_residencySet(texture, resident);
}

void SetResidencyHooks(std::function<bool(rhi::ITexture*)> query, std::function<void(rhi::ITexture*, bool)> set) {
    s_residencyQuery = std::move(query);
    s_residencySet = std::move(set);
}

void RequestTextureStream(rhi::ITexture* texture, uint32_t mipLevelOrQuality) {
    if (s_textureStreamRequest && texture)
        s_textureStreamRequest(texture, mipLevelOrQuality);
}

void SetTextureStreamRequest(std::function<void(rhi::ITexture*, uint32_t)> handler) {
    s_textureStreamRequest = std::move(handler);
}

void RequestMeshLOD(void* meshHandle, uint32_t lodLevel) {
    if (s_meshLODRequest && meshHandle)
        s_meshLODRequest(meshHandle, lodLevel);
}

void SetMeshLODRequest(std::function<void(void*, uint32_t)> handler) {
    s_meshLODRequest = std::move(handler);
}

void RequestAsyncBufferUpload(rhi::IDevice* device, rhi::IBuffer* buffer,
                              size_t offset, size_t size, const void* data,
                              AsyncUploadCallback onComplete) {
    if (s_asyncUpload) {
        s_asyncUpload(device, buffer, offset, size, data, std::move(onComplete));
        return;
    }
    if (buffer && data && size > 0)
        buffer->Update(data, offset, size);
    if (onComplete)
        onComplete();
}

void SetAsyncBufferUpload(std::function<void(rhi::IDevice*, rhi::IBuffer*, size_t, size_t, const void*, AsyncUploadCallback)> impl) {
    s_asyncUpload = std::move(impl);
}

} // namespace render
} // namespace eyelib
