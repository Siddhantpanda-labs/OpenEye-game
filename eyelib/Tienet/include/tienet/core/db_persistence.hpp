/**
 * @file db_persistence.hpp
 * @brief SQLite persistence backend (optional)
 */

#pragma once

#include "tienet/core/channel_persistence.hpp"
#include <string>

namespace tienet {
namespace core {

class SQLiteChannelPersistence : public IChannelPersistence {
public:
    SQLiteChannelPersistence();
    ~SQLiteChannelPersistence() override;

    bool initialize(const std::string& dbPath);

    bool saveChannelState(uint8_t channelId, const PersistentChannelState& state) override;
    bool loadChannelState(uint8_t channelId, PersistentChannelState& state) override;
    bool removeChannelState(uint8_t channelId) override;
    std::vector<uint8_t> getSavedChannelIds() override;
    PersistenceStats getStats() const override { return m_stats; }
    void reset() override;

private:
    std::string m_dbPath;
    void* m_db; // sqlite3*
    PersistenceStats m_stats;
    bool ensureSchema();
};

} // namespace core
} // namespace tienet


