#pragma once

#include "WorldEvents.h"

#include <functional>
#include <unordered_map>

namespace eyelib {
namespace world {

class EventLog;

class EventDispatcher {
public:
    using Handler = std::function<void(const WorldEvent&)>;
    using HandlerId = uint64_t;

    HandlerId AddListener(Handler handler);
    void RemoveListener(HandlerId id);

    void Emit(const WorldEvent& event);

    /** When set, every Emit also appends to the log with getTime() as the event time. */
    void SetEventLog(EventLog* log, std::function<double()> getTime);

private:
    HandlerId m_nextId = 1;
    std::unordered_map<HandlerId, Handler> m_listeners;
    EventLog* m_eventLog = nullptr;
    std::function<double()> m_getTime;
};

} // namespace world
} // namespace eyelib
