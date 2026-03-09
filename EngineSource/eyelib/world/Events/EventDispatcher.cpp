#include "EventDispatcher.h"
#include "EventLog.h"

namespace eyelib {
namespace world {

EventDispatcher::HandlerId EventDispatcher::AddListener(Handler handler) {
    const HandlerId id = m_nextId++;
    m_listeners.emplace(id, std::move(handler));
    return id;
}

void EventDispatcher::RemoveListener(HandlerId id) {
    m_listeners.erase(id);
}

void EventDispatcher::SetEventLog(EventLog* log, std::function<double()> getTime) {
    m_eventLog = log;
    m_getTime = std::move(getTime);
}

void EventDispatcher::Emit(const WorldEvent& event) {
    for (const auto& [id, handler] : m_listeners) {
        (void)id;
        handler(event);
    }
    if (m_eventLog && m_getTime) {
        m_eventLog->Push(event, m_getTime());
    }
}

} // namespace world
} // namespace eyelib
