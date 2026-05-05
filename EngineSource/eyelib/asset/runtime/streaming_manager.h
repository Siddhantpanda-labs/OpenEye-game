#pragma once

#include "asset/core/guid.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace asset {

class AssetDatabase;
class AssetManager;

/**
 * Async / priority-based asset loading. Uses AssetDatabase + AssetManager.
 * Set database and asset root on AssetManager before use.
 */
class StreamingManager {
public:
    /** Request load by path (relative to asset root). Returns Invalid; use GetLoadProgress / IsLoaded. */
    Guid RequestAsset(const std::string& path, int priority = 0);
    void CancelRequest(const std::string& path);
    float GetLoadProgress(const std::string& path) const;
    bool IsLoaded(const std::string& path) const;
    void SetMemoryBudget(size_t bytes) { m_memoryBudget = bytes; }
    size_t GetMemoryBudget() const { return m_memoryBudget; }

private:
    struct Request {
        Guid id{};
        std::atomic<float> progress{0.0f};
        int priority = 0;
        std::atomic<bool> cancelled{false};
    };

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<Request>> m_requests;
    size_t m_memoryBudget = 512ull * 1024ull * 1024ull;

    Guid LoadByExtension(const std::string& path);
};

} // namespace asset
