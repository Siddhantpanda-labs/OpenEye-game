#include "StreamingManager.h"
#include "asset/runtime/streaming_manager.h"

namespace eyelib {
namespace api {

namespace {
AssetId ToApi(const asset::Guid& id) {
    AssetId out;
    out.high = id.high;
    out.low = id.low;
    return out;
}
} // namespace

class StreamingManager::Impl {
public:
    asset::StreamingManager manager;
};

StreamingManager::StreamingManager() : m_impl(std::make_unique<Impl>()) {}
StreamingManager::~StreamingManager() = default;

AssetId StreamingManager::RequestAsset(const std::string& path, int priority) {
    return ToApi(m_impl->manager.RequestAsset(path, priority));
}

void StreamingManager::CancelRequest(const std::string& path) {
    m_impl->manager.CancelRequest(path);
}

float StreamingManager::GetLoadProgress(const std::string& path) const {
    return m_impl->manager.GetLoadProgress(path);
}

} // namespace api
} // namespace eyelib
