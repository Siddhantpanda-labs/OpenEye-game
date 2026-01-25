#include "eyelib/audio/AudioMixer.h"

#include <algorithm>

namespace eyelib {
namespace audio {

AudioGroupHandle AudioMixer::CreateGroup(const std::string& name) {
    const AudioGroupHandle h = m_next++;
    m_groups.emplace(h, Group{ name, 1.0f });
    return h;
}

void AudioMixer::SetGroupVolume(AudioGroupHandle group, float volume) {
    auto it = m_groups.find(group);
    if (it == m_groups.end()) return;
    it->second.volume = std::max(0.0f, volume);
    for (const auto& entry : m_sourceGroups) {
        if (entry.second == group && entry.first) {
            entry.first->SetGroupVolume(it->second.volume);
        }
    }
}

void AudioMixer::AddSourceToGroup(AudioSource* source, AudioGroupHandle group) {
    if (!source) return;
    if (m_groups.find(group) == m_groups.end()) return;
    m_sourceGroups[source] = group;
    source->SetGroupVolume(m_groups[group].volume);
}

float AudioMixer::GetGroupVolume(AudioGroupHandle group) const {
    auto it = m_groups.find(group);
    if (it == m_groups.end()) return 1.0f;
    return it->second.volume;
}

} // namespace audio
} // namespace eyelib
