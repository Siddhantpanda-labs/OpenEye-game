/**
 * @file channel_persistence.hpp
 * @brief Channel persistence and state management
 */

#pragma once

#include "tienet/core/channels.hpp"
#include <string>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>

namespace tienet {
namespace core {

/**
 * @brief Channel persistence modes
 */
enum class PersistenceMode {
    None,           // No persistence
    Memory,         // In-memory only
    File,           // File-based persistence
    Database,       // Database persistence
    Hybrid          // Memory + File/Database
};

/**
 * @brief Channel state for persistence
 */
struct PersistentChannelState {
    uint8_t channelId;
    ChannelConfig config;
    ChannelStats stats;
    std::vector<uint8_t> lastPacket;
    uint32_t lastSequence;
    uint32_t lastTimestamp;
    bool isActive;
    uint32_t reconnectCount;
    uint32_t lastReconnectTime;
    
    PersistentChannelState() : channelId(0), lastSequence(0), lastTimestamp(0),
                              isActive(false), reconnectCount(0), lastReconnectTime(0) {}
};

/**
 * @brief Channel persistence configuration
 */
struct PersistenceConfig {
    PersistenceMode mode;
    std::string persistencePath;
    uint32_t maxStateSize;
    uint32_t saveIntervalMs;
    uint32_t maxReconnectAttempts;
    bool enableAutoReconnect;
    bool enableStateCompression;
    
    PersistenceConfig() : mode(PersistenceMode::Memory), persistencePath("./channel_states"),
                         maxStateSize(1024 * 1024), saveIntervalMs(5000),
                         maxReconnectAttempts(3), enableAutoReconnect(true),
                         enableStateCompression(true) {}
};

/**
 * @brief Channel persistence statistics
 */
struct PersistenceStats {
    uint64_t statesSaved;
    uint64_t statesLoaded;
    uint64_t reconnectAttempts;
    uint64_t successfulReconnects;
    uint64_t failedReconnects;
    uint32_t averageSaveTimeMs;
    uint32_t averageLoadTimeMs;
    
    PersistenceStats() : statesSaved(0), statesLoaded(0), reconnectAttempts(0),
                        successfulReconnects(0), failedReconnects(0),
                        averageSaveTimeMs(0), averageLoadTimeMs(0) {}
};

/**
 * @brief Abstract channel persistence interface
 */
class IChannelPersistence {
public:
    virtual ~IChannelPersistence() = default;
    
    /**
     * @brief Save channel state
     * @param channelId Channel ID
     * @param state Channel state
     * @return true if successful, false otherwise
     */
    virtual bool saveChannelState(uint8_t channelId, const PersistentChannelState& state) = 0;
    
    /**
     * @brief Load channel state
     * @param channelId Channel ID
     * @param state Output channel state
     * @return true if successful, false otherwise
     */
    virtual bool loadChannelState(uint8_t channelId, PersistentChannelState& state) = 0;
    
    /**
     * @brief Remove channel state
     * @param channelId Channel ID
     * @return true if successful, false otherwise
     */
    virtual bool removeChannelState(uint8_t channelId) = 0;
    
    /**
     * @brief Get all saved channel IDs
     * @return Vector of channel IDs
     */
    virtual std::vector<uint8_t> getSavedChannelIds() = 0;
    
    /**
     * @brief Get persistence statistics
     * @return Persistence statistics
     */
    virtual PersistenceStats getStats() const = 0;
    
    /**
     * @brief Reset persistence
     */
    virtual void reset() = 0;
};

/**
 * @brief Memory-based channel persistence
 */
class MemoryChannelPersistence : public IChannelPersistence {
public:
    MemoryChannelPersistence();
    ~MemoryChannelPersistence() override = default;
    
    bool saveChannelState(uint8_t channelId, const PersistentChannelState& state) override;
    bool loadChannelState(uint8_t channelId, PersistentChannelState& state) override;
    bool removeChannelState(uint8_t channelId) override;
    std::vector<uint8_t> getSavedChannelIds() override;
    PersistenceStats getStats() const override { return m_stats; }
    void reset() override;

private:
    std::unordered_map<uint8_t, PersistentChannelState> m_channelStates;
    PersistenceStats m_stats;
    mutable std::mutex m_mutex;
};

/**
 * @brief File-based channel persistence
 */
class FileChannelPersistence : public IChannelPersistence {
public:
    FileChannelPersistence(const std::string& basePath);
    ~FileChannelPersistence() override = default;
    
    bool saveChannelState(uint8_t channelId, const PersistentChannelState& state) override;
    bool loadChannelState(uint8_t channelId, PersistentChannelState& state) override;
    bool removeChannelState(uint8_t channelId) override;
    std::vector<uint8_t> getSavedChannelIds() override;
    PersistenceStats getStats() const override { return m_stats; }
    void reset() override;

private:
    std::string m_basePath;
    PersistenceStats m_stats;
    mutable std::mutex m_mutex;
    
    std::string getChannelFilePath(uint8_t channelId);
    bool serializeState(const PersistentChannelState& state, std::vector<uint8_t>& data);
    bool deserializeState(const std::vector<uint8_t>& data, PersistentChannelState& state);
};

/**
 * @brief Hybrid channel persistence (Memory + File)
 */
class HybridChannelPersistence : public IChannelPersistence {
public:
    HybridChannelPersistence(const std::string& basePath);
    ~HybridChannelPersistence() override = default;
    
    bool saveChannelState(uint8_t channelId, const PersistentChannelState& state) override;
    bool loadChannelState(uint8_t channelId, PersistentChannelState& state) override;
    bool removeChannelState(uint8_t channelId) override;
    std::vector<uint8_t> getSavedChannelIds() override;
    PersistenceStats getStats() const override;
    void reset() override;
    
    /**
     * @brief Set hybrid parameters
     * @param memoryThreshold Threshold for memory-only storage
     * @param enableCompression Enable compression for file storage
     */
    void setHybridParams(uint32_t memoryThreshold, bool enableCompression);

private:
    std::unique_ptr<MemoryChannelPersistence> m_memoryPersistence;
    std::unique_ptr<FileChannelPersistence> m_filePersistence;
    PersistenceStats m_stats;
    uint32_t m_memoryThreshold;
    bool m_enableCompression;
};

/**
 * @brief Channel persistence manager
 */
class ChannelPersistenceManager {
public:
    ChannelPersistenceManager();
    ~ChannelPersistenceManager();
    
    /**
     * @brief Initialize persistence manager
     * @param config Persistence configuration
     * @return true if successful, false otherwise
     */
    bool initialize(const PersistenceConfig& config);
    
    /**
     * @brief Shutdown persistence manager
     */
    void shutdown();
    
    /**
     * @brief Save channel state
     * @param channelId Channel ID
     * @param state Channel state
     * @return true if successful, false otherwise
     */
    bool saveChannelState(uint8_t channelId, const PersistentChannelState& state);
    
    /**
     * @brief Load channel state
     * @param channelId Channel ID
     * @param state Output channel state
     * @return true if successful, false otherwise
     */
    bool loadChannelState(uint8_t channelId, PersistentChannelState& state);
    
    /**
     * @brief Remove channel state
     * @param channelId Channel ID
     * @return true if successful, false otherwise
     */
    bool removeChannelState(uint8_t channelId);
    
    /**
     * @brief Get all saved channel IDs
     * @return Vector of channel IDs
     */
    std::vector<uint8_t> getSavedChannelIds();
    
    /**
     * @brief Start automatic persistence
     */
    void startAutoPersistence();
    
    /**
     * @brief Stop automatic persistence
     */
    void stopAutoPersistence();
    
    /**
     * @brief Update persistence (called periodically)
     */
    void update();
    
    /**
     * @brief Get persistence statistics
     * @return Persistence statistics
     */
    PersistenceStats getStats() const;
    
    /**
     * @brief Set channel state callback
     * @param callback Callback for channel state changes
     */
    void setChannelStateCallback(std::function<void(uint8_t, const PersistentChannelState&)> callback);

private:
    std::unique_ptr<IChannelPersistence> m_persistence;
    PersistenceConfig m_config;
    std::atomic<bool> m_running;
    std::thread m_persistenceThread;
    std::chrono::steady_clock::time_point m_lastSaveTime;
    std::function<void(uint8_t, const PersistentChannelState&)> m_stateCallback;
    
    void persistenceLoop();
    void autoSave();
};

/**
 * @brief Channel reconnection manager
 */
class ChannelReconnectionManager {
public:
    ChannelReconnectionManager();
    ~ChannelReconnectionManager();
    
    /**
     * @brief Initialize reconnection manager
     * @param persistenceManager Persistence manager
     * @return true if successful, false otherwise
     */
    bool initialize(ChannelPersistenceManager* persistenceManager);
    
    /**
     * @brief Shutdown reconnection manager
     */
    void shutdown();
    
    /**
     * @brief Register channel for reconnection
     * @param channelId Channel ID
     * @param config Channel configuration
     * @return true if successful, false otherwise
     */
    bool registerChannel(uint8_t channelId, const ChannelConfig& config);
    
    /**
     * @brief Unregister channel
     * @param channelId Channel ID
     */
    void unregisterChannel(uint8_t channelId);
    
    /**
     * @brief Attempt reconnection for a channel
     * @param channelId Channel ID
     * @return true if reconnection successful, false otherwise
     */
    bool attemptReconnection(uint8_t channelId);
    
    /**
     * @brief Update reconnection manager
     */
    void update();
    
    /**
     * @brief Get reconnection statistics
     * @return Reconnection statistics
     */
    PersistenceStats getStats() const;

private:
    ChannelPersistenceManager* m_persistenceManager;
    std::unordered_map<uint8_t, ChannelConfig> m_registeredChannels;
    std::unordered_map<uint8_t, uint32_t> m_reconnectAttempts;
    std::unordered_map<uint8_t, uint32_t> m_lastReconnectTime;
    PersistenceStats m_stats;
    std::atomic<bool> m_running;
};

} // namespace core
} // namespace tienet
