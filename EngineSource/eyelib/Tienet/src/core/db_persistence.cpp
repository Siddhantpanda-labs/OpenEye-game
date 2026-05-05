/**
 * @file db_persistence.cpp
 */

#include "tienet/core/db_persistence.hpp"
#include <vector>
#include <cstring>

#ifdef TIENET_HAS_SQLITE3
#include <sqlite3.h>
#endif

namespace tienet {
namespace core {

SQLiteChannelPersistence::SQLiteChannelPersistence() : m_db(nullptr) {}
SQLiteChannelPersistence::~SQLiteChannelPersistence() {
#ifdef TIENET_HAS_SQLITE3
    if (m_db) {
        sqlite3_close(reinterpret_cast<sqlite3*>(m_db));
        m_db = nullptr;
    }
#endif
}

bool SQLiteChannelPersistence::initialize(const std::string& dbPath) {
    m_dbPath = dbPath;
#ifdef TIENET_HAS_SQLITE3
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        return false;
    }
    m_db = db;
    return ensureSchema();
#else
    (void)dbPath; return false;
#endif
}

bool SQLiteChannelPersistence::ensureSchema() {
#ifdef TIENET_HAS_SQLITE3
    const char* sql = "CREATE TABLE IF NOT EXISTS channel_state (\n"
                      "  channel_id INTEGER PRIMARY KEY,\n"
                      "  data BLOB\n"
                      ");";
    char* err = nullptr;
    if (sqlite3_exec(reinterpret_cast<sqlite3*>(m_db), sql, nullptr, nullptr, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool SQLiteChannelPersistence::saveChannelState(uint8_t channelId, const PersistentChannelState& state) {
#ifdef TIENET_HAS_SQLITE3
    std::vector<uint8_t> blob;
    // Reuse File serializer format
    {
        // Serialize minimal fields
        blob.push_back(state.channelId);
        blob.push_back(static_cast<uint8_t>(state.config.deliveryMode));
        blob.push_back(static_cast<uint8_t>(state.config.priority));
        uint32_t lastSeq = state.lastSequence;
        for (int i = 0; i < 4; i++) blob.push_back(static_cast<uint8_t>(lastSeq >> (i*8)));
        uint32_t packetSize = static_cast<uint32_t>(state.lastPacket.size());
        for (int i = 0; i < 4; i++) blob.push_back(static_cast<uint8_t>(packetSize >> (i*8)));
        blob.insert(blob.end(), state.lastPacket.begin(), state.lastPacket.end());
    }

    const char* sql = "REPLACE INTO channel_state(channel_id, data) VALUES(?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(reinterpret_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, static_cast<int>(channelId));
    sqlite3_bind_blob(stmt, 2, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    if (ok) m_stats.statesSaved++;
    return ok;
#else
    (void)channelId; (void)state; return false;
#endif
}

bool SQLiteChannelPersistence::loadChannelState(uint8_t channelId, PersistentChannelState& state) {
#ifdef TIENET_HAS_SQLITE3
    const char* sql = "SELECT data FROM channel_state WHERE channel_id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(reinterpret_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, static_cast<int>(channelId));
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* data = sqlite3_column_blob(stmt, 0);
        int size = sqlite3_column_bytes(stmt, 0);
        std::vector<uint8_t> blob(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
        // Minimal deserialize
        size_t offset = 0;
        state.channelId = blob[offset++];
        state.config.deliveryMode = static_cast<DeliveryMode>(blob[offset++]);
        state.config.priority = static_cast<ChannelPriority>(blob[offset++]);
        state.lastSequence = 0; for (int i=0;i<4;i++) state.lastSequence |= (static_cast<uint32_t>(blob[offset++]) << (i*8));
        uint32_t packetSize = 0; for (int i=0;i<4;i++) packetSize |= (static_cast<uint32_t>(blob[offset++]) << (i*8));
        if (offset + packetSize <= blob.size()) {
            state.lastPacket.assign(blob.begin() + offset, blob.begin() + offset + packetSize);
            ok = true;
        }
    }
    sqlite3_finalize(stmt);
    if (ok) m_stats.statesLoaded++;
    return ok;
#else
    (void)channelId; (void)state; return false;
#endif
}

bool SQLiteChannelPersistence::removeChannelState(uint8_t channelId) {
#ifdef TIENET_HAS_SQLITE3
    const char* sql = "DELETE FROM channel_state WHERE channel_id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(reinterpret_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, static_cast<int>(channelId));
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)channelId; return false;
#endif
}

std::vector<uint8_t> SQLiteChannelPersistence::getSavedChannelIds() {
#ifdef TIENET_HAS_SQLITE3
    std::vector<uint8_t> ids;
    const char* sql = "SELECT channel_id FROM channel_state;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(reinterpret_cast<sqlite3*>(m_db), sql, -1, &stmt, nullptr) != SQLITE_OK) return ids;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ids.push_back(static_cast<uint8_t>(sqlite3_column_int(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return ids;
#else
    return {};
#endif
}

void SQLiteChannelPersistence::reset() {
#ifdef TIENET_HAS_SQLITE3
    const char* sql = "DELETE FROM channel_state;";
    sqlite3_exec(reinterpret_cast<sqlite3*>(m_db), sql, nullptr, nullptr, nullptr);
#endif
    m_stats = PersistenceStats();
}

} // namespace core
} // namespace tienet


