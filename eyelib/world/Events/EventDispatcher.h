#pragma once

#include "WorldEvents.h"

#include <functional>
#include <unordered_map>

namespace eyelib {
namespace world {

class EventDispatcher {
public:
    using Handler = std::function<void(const WorldEvent&)>;
    using HandlerId = uint64_t;

    HandlerId AddListener(Handler handler);
    void RemoveListener(HandlerId id);

    void Emit(const WorldEvent& event);

private:
    HandlerId m_nextId = 1;
    std::unordered_map<HandlerId, Handler> m_listeners;
};

} // namespace world
} // namespace eyelib
