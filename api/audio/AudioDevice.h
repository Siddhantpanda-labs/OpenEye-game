#pragma once

namespace eyelib {
namespace audio { class AudioDevice; }
namespace api {

class AudioDevice {
public:
    AudioDevice();
    ~AudioDevice();

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    bool Initialize();
    void Shutdown();
    void SetMasterVolume(float volume);
    float GetMasterVolume() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib
