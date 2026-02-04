#pragma once

#include "PlayerConfig.h"
#include "api/world/All.h"
#include "eyelib/audio/AudioDevice.h"
#include "eyelib/audio/AudioListener.h"

namespace openeye::player {

class WorldAudioBridge {
public:
    bool Initialize();
    void Shutdown();

    void BuildFromWorld(eyelib::api::world::World& world);
    void Update(eyelib::api::world::World& world, float deltaSeconds);

private:
    eyelib::audio::AudioDevice m_device;
    eyelib::audio::AudioListener m_listener;
    bool m_ready = false;
};

} // namespace openeye::player
