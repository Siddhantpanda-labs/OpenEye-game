/**
 * @file channel_persistence.cpp
 * @brief Channel persistence implementation
 */

#include "tienet/core/channel_persistence.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace tienet {
namespace core {

// Memory Channel Persistence implementation
MemoryChannelPersistence::MemoryChannelPersistence() {
}

bool MemoryChannelPersistence::saveChannelState(uint8_t channelId, const PersistentChannelState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    m_channelStates[channelId] = state;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    m_stats.statesSaved++;
    m_stats.averageSaveTimeMs = (m_stats.averageSaveTimeMs * (m_stats.statesSaved - 1) + duration.count()) / m_stats.statesSaved;
    
    return true;
}

bool MemoryChannelPersistence::loadChannelState(uint8_t channelId, PersistentChannelState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    auto it = m_channelStates.find(channelId);
    if (it == m_channelStates.end()) {
        return false;
    }
    
    state = it->second;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    m_stats.statesLoaded++;
    m_stats.averageLoadTimeMs = (m_stats.averageLoadTimeMs * (m_stats.statesLoaded - 1) + duration.count()) / m_stats.statesLoaded;
    
    return true;
}

bool MemoryChannelPersistence::removeChannelState(uint8_t channelId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_channelStates.find(channelId);
    if (it == m_channelStates.end()) {
        return false;
    }
    
    m_channelStates.erase(it);
    return true;
}

std::vector<uint8_t> MemoryChannelPersistence::getSavedChannelIds() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<uint8_t> channelIds;
    channelIds.reserve(m_channelStates.size());
    
    for (const auto& pair : m_channelStates) {
        channelIds.push_back(pair.first);
    }
    
    return channelIds;
}

void MemoryChannelPersistence::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_channelStates.clear();
    m_stats = PersistenceStats();
}

// File Channel Persistence implementation
FileChannelPersistence::FileChannelPersistence(const std::string& basePath) 
    : m_basePath(basePath) {
    // Create directory if it doesn't exist
    std::filesystem::create_directories(m_basePath);
}

bool FileChannelPersistence::saveChannelState(uint8_t channelId, const PersistentChannelState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string filePath = getChannelFilePath(channelId);
    std::ofstream file(filePath, std::ios::binary);
    
    if (!file.is_open()) {
        return false;
    }
    
    std::vector<uint8_t> data;
    if (!serializeState(state, data)) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    m_stats.statesSaved++;
    m_stats.averageSaveTimeMs = (m_stats.averageSaveTimeMs * (m_stats.statesSaved - 1) + duration.count()) / m_stats.statesSaved;
    
    return true;
}

bool FileChannelPersistence::loadChannelState(uint8_t channelId, PersistentChannelState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string filePath = getChannelFilePath(channelId);
    std::ifstream file(filePath, std::ios::binary);
    
    if (!file.is_open()) {
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();
    
    if (!deserializeState(data, state)) {
        return false;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    m_stats.statesLoaded++;
    m_stats.averageLoadTimeMs = (m_stats.averageLoadTimeMs * (m_stats.statesLoaded - 1) + duration.count()) / m_stats.statesLoaded;
    
    return true;
}

bool FileChannelPersistence::removeChannelState(uint8_t channelId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string filePath = getChannelFilePath(channelId);
    return std::filesystem::remove(filePath);
}

std::vector<uint8_t> FileChannelPersistence::getSavedChannelIds() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<uint8_t> channelIds;
    
    for (const auto& entry : std::filesystem::directory_iterator(m_basePath)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.substr(0, 8) == "channel_") {
                try {
                    uint8_t channelId = static_cast<uint8_t>(std::stoi(filename.substr(8)));
                    channelIds.push_back(channelId);
                } catch (const std::exception&) {
                    // Invalid filename, skip
                }
            }
        }
    }
    
    return channelIds;
}

void FileChannelPersistence::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Remove all channel files
    for (const auto& entry : std::filesystem::directory_iterator(m_basePath)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.substr(0, 8) == "channel_") {
                std::filesystem::remove(entry.path());
            }
        }
    }
    
    m_stats = PersistenceStats();
}

std::string FileChannelPersistence::getChannelFilePath(uint8_t channelId) {
    return m_basePath + "/channel_" + std::to_string(channelId) + ".state";
}

bool FileChannelPersistence::serializeState(const PersistentChannelState& state, std::vector<uint8_t>& data) {
    // Simple binary serialization
    data.clear();
    
    // Channel ID
    data.push_back(state.channelId);
    
    // Config
    data.push_back(static_cast<uint8_t>(state.config.deliveryMode));
    data.push_back(static_cast<uint8_t>(state.config.priority));
    
    // Stats
    uint64_t packetsSent = state.stats.packetsSent;
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(packetsSent >> (i * 8)));
    }
    
    // Last packet
    uint32_t packetSize = static_cast<uint32_t>(state.lastPacket.size());
    for (int i = 0; i < 4; i++) {
        data.push_back(static_cast<uint8_t>(packetSize >> (i * 8)));
    }
    data.insert(data.end(), state.lastPacket.begin(), state.lastPacket.end());
    
    // Other fields
    uint32_t lastSeq = state.lastSequence;
    for (int i = 0; i < 4; i++) {
        data.push_back(static_cast<uint8_t>(lastSeq >> (i * 8)));
    }
    
    return true;
}

bool FileChannelPersistence::deserializeState(const std::vector<uint8_t>& data, PersistentChannelState& state) {
    if (data.size() < 20) { // Minimum size check
        return false;
    }
    
    size_t offset = 0;
    
    // Channel ID
    state.channelId = data[offset++];
    
    // Config
    state.config.deliveryMode = static_cast<DeliveryMode>(data[offset++]);
    state.config.priority = static_cast<ChannelPriority>(data[offset++]);
    
    // Stats
    state.stats.packetsSent = 0;
    for (int i = 0; i < 8; i++) {
        state.stats.packetsSent |= (static_cast<uint64_t>(data[offset++]) << (i * 8));
    }
    
    // Last packet
    uint32_t packetSize = 0;
    for (int i = 0; i < 4; i++) {
        packetSize |= (static_cast<uint32_t>(data[offset++]) << (i * 8));
    }
    
    if (offset + packetSize > data.size()) {
        return false;
    }
    
    state.lastPacket.assign(data.begin() + offset, data.begin() + offset + packetSize);
    offset += packetSize;
    
    // Other fields
    state.lastSequence = 0;
    for (int i = 0; i < 4; i++) {
        state.lastSequence |= (static_cast<uint32_t>(data[offset++]) << (i * 8));
    }
    
    return true;
}

// Hybrid Channel Persistence implementation
HybridChannelPersistence::HybridChannelPersistence(const std::string& basePath) 
    : m_memoryThreshold(1024), m_enableCompression(true) {
    m_memoryPersistence = std::make_unique<MemoryChannelPersistence>();
    m_filePersistence = std::make_unique<FileChannelPersistence>(basePath);
}

bool HybridChannelPersistence::saveChannelState(uint8_t channelId, const PersistentChannelState& state) {
    // Always save to memory
    bool memoryResult = m_memoryPersistence->saveChannelState(channelId, state);
    
    // Save to file if state is large enough
    if (state.lastPacket.size() >= m_memoryThreshold) {
        m_filePersistence->saveChannelState(channelId, state);
    }
    
    return memoryResult;
}

bool HybridChannelPersistence::loadChannelState(uint8_t channelId, PersistentChannelState& state) {
    // Try memory first
    if (m_memoryPersistence->loadChannelState(channelId, state)) {
        return true;
    }
    
    // Fall back to file
    return m_filePersistence->loadChannelState(channelId, state);
}

bool HybridChannelPersistence::removeChannelState(uint8_t channelId) {
    bool memoryResult = m_memoryPersistence->removeChannelState(channelId);
    bool fileResult = m_filePersistence->removeChannelState(channelId);
    
    return memoryResult || fileResult;
}

std::vector<uint8_t> HybridChannelPersistence::getSavedChannelIds() {
    auto memoryIds = m_memoryPersistence->getSavedChannelIds();
    auto fileIds = m_filePersistence->getSavedChannelIds();
    
    // Merge and deduplicate
    std::vector<uint8_t> allIds;
    allIds.insert(allIds.end(), memoryIds.begin(), memoryIds.end());
    allIds.insert(allIds.end(), fileIds.begin(), fileIds.end());
    
    std::sort(allIds.begin(), allIds.end());
    allIds.erase(std::unique(allIds.begin(), allIds.end()), allIds.end());
    
    return allIds;
}

PersistenceStats HybridChannelPersistence::getStats() const {
    auto memoryStats = m_memoryPersistence->getStats();
    auto fileStats = m_filePersistence->getStats();
    
    PersistenceStats combined;
    combined.statesSaved = memoryStats.statesSaved + fileStats.statesSaved;
    combined.statesLoaded = memoryStats.statesLoaded + fileStats.statesLoaded;
    combined.reconnectAttempts = memoryStats.reconnectAttempts + fileStats.reconnectAttempts;
    combined.successfulReconnects = memoryStats.successfulReconnects + fileStats.successfulReconnects;
    combined.failedReconnects = memoryStats.failedReconnects + fileStats.failedReconnects;
    combined.averageSaveTimeMs = (memoryStats.averageSaveTimeMs + fileStats.averageSaveTimeMs) / 2;
    combined.averageLoadTimeMs = (memoryStats.averageLoadTimeMs + fileStats.averageLoadTimeMs) / 2;
    
    return combined;
}

void HybridChannelPersistence::reset() {
    m_memoryPersistence->reset();
    m_filePersistence->reset();
}

void HybridChannelPersistence::setHybridParams(uint32_t memoryThreshold, bool enableCompression) {
    m_memoryThreshold = memoryThreshold;
    m_enableCompression = enableCompression;
}

// Channel Persistence Manager implementation
ChannelPersistenceManager::ChannelPersistenceManager() 
    : m_running(false) {
}

ChannelPersistenceManager::~ChannelPersistenceManager() {
    shutdown();
}

bool ChannelPersistenceManager::initialize(const PersistenceConfig& config) {
    m_config = config;
    
    switch (config.mode) {
        case PersistenceMode::Memory:
            m_persistence = std::make_unique<MemoryChannelPersistence>();
            break;
        case PersistenceMode::File:
            m_persistence = std::make_unique<FileChannelPersistence>(config.persistencePath);
            break;
        case PersistenceMode::Hybrid:
            m_persistence = std::make_unique<HybridChannelPersistence>(config.persistencePath);
            break;
        default:
            m_persistence = std::make_unique<MemoryChannelPersistence>();
            break;
    }
    
    return true;
}

void ChannelPersistenceManager::shutdown() {
    stopAutoPersistence();
    m_persistence.reset();
}

bool ChannelPersistenceManager::saveChannelState(uint8_t channelId, const PersistentChannelState& state) {
    if (!m_persistence) {
        return false;
    }
    
    return m_persistence->saveChannelState(channelId, state);
}

bool ChannelPersistenceManager::loadChannelState(uint8_t channelId, PersistentChannelState& state) {
    if (!m_persistence) {
        return false;
    }
    
    return m_persistence->loadChannelState(channelId, state);
}

bool ChannelPersistenceManager::removeChannelState(uint8_t channelId) {
    if (!m_persistence) {
        return false;
    }
    
    return m_persistence->removeChannelState(channelId);
}

std::vector<uint8_t> ChannelPersistenceManager::getSavedChannelIds() {
    if (!m_persistence) {
        return {};
    }
    
    return m_persistence->getSavedChannelIds();
}

void ChannelPersistenceManager::startAutoPersistence() {
    if (m_running) {
        return;
    }
    
    m_running = true;
    m_persistenceThread = std::thread(&ChannelPersistenceManager::persistenceLoop, this);
}

void ChannelPersistenceManager::stopAutoPersistence() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    if (m_persistenceThread.joinable()) {
        m_persistenceThread.join();
    }
}

void ChannelPersistenceManager::update() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastSaveTime);
    
    if (elapsed.count() >= m_config.saveIntervalMs) {
        autoSave();
        m_lastSaveTime = now;
    }
}

PersistenceStats ChannelPersistenceManager::getStats() const {
    if (!m_persistence) {
        return PersistenceStats();
    }
    
    return m_persistence->getStats();
}

void ChannelPersistenceManager::setChannelStateCallback(std::function<void(uint8_t, const PersistentChannelState&)> callback) {
    m_stateCallback = callback;
}

void ChannelPersistenceManager::persistenceLoop() {
    while (m_running) {
        autoSave();
        std::this_thread::sleep_for(std::chrono::milliseconds(m_config.saveIntervalMs));
    }
}

void ChannelPersistenceManager::autoSave() {
    // This would typically iterate through active channels and save their states
    // For now, it's a placeholder
}

// Channel Reconnection Manager implementation
ChannelReconnectionManager::ChannelReconnectionManager() 
    : m_persistenceManager(nullptr), m_running(false) {
}

ChannelReconnectionManager::~ChannelReconnectionManager() {
    shutdown();
}

bool ChannelReconnectionManager::initialize(ChannelPersistenceManager* persistenceManager) {
    m_persistenceManager = persistenceManager;
    return true;
}

void ChannelReconnectionManager::shutdown() {
    m_running = false;
    m_registeredChannels.clear();
    m_reconnectAttempts.clear();
    m_lastReconnectTime.clear();
}

bool ChannelReconnectionManager::registerChannel(uint8_t channelId, const ChannelConfig& config) {
    m_registeredChannels[channelId] = config;
    m_reconnectAttempts[channelId] = 0;
    m_lastReconnectTime[channelId] = 0;
    
    return true;
}

void ChannelReconnectionManager::unregisterChannel(uint8_t channelId) {
    m_registeredChannels.erase(channelId);
    m_reconnectAttempts.erase(channelId);
    m_lastReconnectTime.erase(channelId);
}

bool ChannelReconnectionManager::attemptReconnection(uint8_t channelId) {
    auto it = m_registeredChannels.find(channelId);
    if (it == m_registeredChannels.end()) {
        return false;
    }
    
    uint32_t currentTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    
    // Check if enough time has passed since last attempt
    if (currentTime - m_lastReconnectTime[channelId] < 5000) { // 5 seconds
        return false;
    }
    
    m_reconnectAttempts[channelId]++;
    m_lastReconnectTime[channelId] = currentTime;
    m_stats.reconnectAttempts++;
    
    // Try to load channel state
    PersistentChannelState state;
    if (m_persistenceManager && m_persistenceManager->loadChannelState(channelId, state)) {
        m_stats.successfulReconnects++;
        return true;
    }
    
    m_stats.failedReconnects++;
    return false;
}

void ChannelReconnectionManager::update() {
    uint32_t currentTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    
    for (auto& pair : m_registeredChannels) {
        uint8_t channelId = pair.first;
        
        // Attempt reconnection if needed
        if (m_reconnectAttempts[channelId] < 3) { // Max 3 attempts
            attemptReconnection(channelId);
        }
    }
}

PersistenceStats ChannelReconnectionManager::getStats() const {
    return m_stats;
}

} // namespace core
} // namespace tienet
