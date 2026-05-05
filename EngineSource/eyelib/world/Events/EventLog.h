#pragma once

#include "WorldEvents.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace world {

/** Entry in the event log: sequence id, simulation time, and event. */
struct EventLogEntry {
    uint64_t sequenceId = 0;
    double time = 0.0;
    WorldEvent event{};
};

/** Ring buffer of world events for introspection and AI observation. Events are appended on each Emit when the dispatcher is wired. */
class EventLog {
public:
    static constexpr size_t kDefaultMaxEntries = 65536;

    explicit EventLog(size_t maxEntries = kDefaultMaxEntries);

    /** Append an event with the given simulation time. Returns the sequence id assigned. */
    uint64_t Push(const WorldEvent& event, double simulationTime);

    /** Return all events with sequenceId > sinceSequenceId, in order. Also returns the latest sequence id (for next poll). */
    std::vector<EventLogEntry> GetEventsSince(uint64_t sinceSequenceId) const;

    /** Return all events with time > sinceTime, in order. */
    std::vector<EventLogEntry> GetEventsSinceTime(double sinceTime) const;

    /** Current next sequence id (one past the last pushed). */
    uint64_t GetLastSequenceId() const { return m_nextSequenceId; }

    void Clear();

private:
    size_t m_maxEntries = kDefaultMaxEntries;
    uint64_t m_nextSequenceId = 1;
    std::vector<EventLogEntry> m_entries;
    size_t m_head = 0;
    size_t m_size = 0;
};

} // namespace world
} // namespace eyelib
