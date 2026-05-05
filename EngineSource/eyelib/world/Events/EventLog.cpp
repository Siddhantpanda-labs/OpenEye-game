#include "EventLog.h"

namespace eyelib {
namespace world {

EventLog::EventLog(size_t maxEntries)
    : m_maxEntries(maxEntries > 0 ? maxEntries : kDefaultMaxEntries)
    , m_entries(m_maxEntries) {}

uint64_t EventLog::Push(const WorldEvent& event, double simulationTime) {
    const uint64_t seq = m_nextSequenceId++;
    EventLogEntry entry;
    entry.sequenceId = seq;
    entry.time = simulationTime;
    entry.event = event;
    if (m_maxEntries == 0) return seq;
    const size_t idx = m_head % m_maxEntries;
    m_entries[idx] = std::move(entry);
    m_head++;
    if (m_size < m_maxEntries) m_size++;
    return seq;
}

std::vector<EventLogEntry> EventLog::GetEventsSince(uint64_t sinceSequenceId) const {
    std::vector<EventLogEntry> out;
    if (m_size == 0) return out;
    const size_t startIdx = (m_size >= m_maxEntries) ? (m_head % m_maxEntries) : 0;
    for (size_t i = 0; i < m_size; ++i) {
        const size_t idx = (startIdx + i) % m_maxEntries;
        const auto& e = m_entries[idx];
        if (e.sequenceId > sinceSequenceId) {
            out.push_back(e);
        }
    }
    return out;
}

std::vector<EventLogEntry> EventLog::GetEventsSinceTime(double sinceTime) const {
    std::vector<EventLogEntry> out;
    if (m_size == 0) return out;
    const size_t startIdx = (m_size >= m_maxEntries) ? (m_head % m_maxEntries) : 0;
    for (size_t i = 0; i < m_size; ++i) {
        const size_t idx = (startIdx + i) % m_maxEntries;
        const auto& e = m_entries[idx];
        if (e.time > sinceTime) {
            out.push_back(e);
        }
    }
    return out;
}

void EventLog::Clear() {
    m_nextSequenceId = 1;
    m_head = 0;
    m_size = 0;
}

} // namespace world
} // namespace eyelib
