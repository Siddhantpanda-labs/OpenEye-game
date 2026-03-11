#include "StreamingManager.h"

#include "eyelib/asset/StreamingManager.h"

namespace eyelib {
namespace api {

namespace {
AssetId ToApi(const assets::AssetId& id) {
    AssetId out;
    out.high = id.High();
    out.low = id.Low();
    return out;
}
} // namespace

class StreamingManager::Impl {
public:
    assets::StreamingManager manager;
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
