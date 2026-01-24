#pragma once

#include "eyelib/assets/AssetId.h"
#include "eyelib/assets/AssetManager.h"

#include <atomic>
#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>

namespace eyelib {
namespace assets {

class StreamingManager {
public:
    AssetId RequestAsset(const std::string& path, int priority = 0);
    void CancelRequest(const std::string& path);
    float GetLoadProgress(const std::string& path) const;
    bool IsLoaded(const std::string& path) const;
    void SetMemoryBudget(size_t bytes) { m_memoryBudget = bytes; }
    size_t GetMemoryBudget() const { return m_memoryBudget; }

private:
    struct Request {
        AssetId id{};
        std::atomic<float> progress{0.0f};
        int priority = 0;
        std::atomic<bool> cancelled{false};
    };

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<Request>> m_requests;
    size_t m_memoryBudget = 512ull * 1024ull * 1024ull;

    AssetId LoadByExtension(const std::string& path);
};

} // namespace assets
} // namespace eyelib
