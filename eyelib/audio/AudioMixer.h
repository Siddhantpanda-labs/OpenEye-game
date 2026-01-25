#pragma once

#include "eyelib/audio/AudioSource.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace eyelib {
namespace audio {

using AudioGroupHandle = uint32_t;

/**
 * @brief Simple audio mixer/grouping (placeholder).
 */
class AudioMixer {
public:
    AudioGroupHandle CreateGroup(const std::string& name);
    void SetGroupVolume(AudioGroupHandle group, float volume);
    void AddSourceToGroup(AudioSource* source, AudioGroupHandle group);

    float GetGroupVolume(AudioGroupHandle group) const;

private:
    struct Group {
        std::string name;
        float volume = 1.0f;
    };

    AudioGroupHandle m_next = 1;
    std::unordered_map<AudioGroupHandle, Group> m_groups;
    std::unordered_map<AudioSource*, AudioGroupHandle> m_sourceGroups;
};

} // namespace audio
} // namespace eyelib
