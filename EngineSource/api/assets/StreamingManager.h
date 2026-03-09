#pragma once

#include "api/assets/AssetId.h"

#include <memory>
#include <string>

namespace eyelib {
namespace api {

class StreamingManager {
public:
    StreamingManager();
    ~StreamingManager();

    AssetId RequestAsset(const std::string& path, int priority = 0);
    void CancelRequest(const std::string& path);
    float GetLoadProgress(const std::string& path) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
