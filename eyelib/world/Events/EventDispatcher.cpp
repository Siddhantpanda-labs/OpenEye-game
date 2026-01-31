#include "EventDispatcher.h"

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

void EventDispatcher::Emit(const WorldEvent& event) {
    for (const auto& [id, handler] : m_listeners) {
        (void)id;
        handler(event);
    }
}

} // namespace world
} // namespace eyelib
