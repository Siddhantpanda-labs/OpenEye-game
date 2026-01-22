#include "AnimEventTrack.h"
#include <algorithm>

namespace OpenEye {
namespace Animation {
namespace Controller {

void AnimEventTrack::SetEvents(const std::string& clipName, std::vector<AnimEvent> events) {
    std::sort(events.begin(), events.end(), [](const AnimEvent& a, const AnimEvent& b) {
        return a.time < b.time;
    });
    m_eventsByClip[clipName] = std::move(events);
}

const std::vector<AnimEvent>* AnimEventTrack::GetEvents(const std::string& clipName) const {
    auto it = m_eventsByClip.find(clipName);
    if (it == m_eventsByClip.end()) return nullptr;
    return &it->second;
}

} // namespace Controller
} // namespace Animation
} // namespace OpenEye

