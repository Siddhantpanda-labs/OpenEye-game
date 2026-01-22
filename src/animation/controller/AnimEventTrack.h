#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace OpenEye {
namespace Animation {
namespace Controller {

struct AnimEvent {
    float time = 0.0f;           // seconds in clip space
    std::string name;            // event id
    std::string payload;         // optional payload (string for now)
};

// ClipName -> sorted events
class AnimEventTrack {
public:
    void SetEvents(const std::string& clipName, std::vector<AnimEvent> events);
    const std::vector<AnimEvent>* GetEvents(const std::string& clipName) const;

private:
    std::unordered_map<std::string, std::vector<AnimEvent>> m_eventsByClip;
};

} // namespace Controller
} // namespace Animation
} // namespace OpenEye

