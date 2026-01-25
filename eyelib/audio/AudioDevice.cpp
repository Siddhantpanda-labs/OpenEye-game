#include "eyelib/audio/AudioDevice.h"

#include "eyelib/core/Log.h"

#include <AL/al.h>
#include <AL/alc.h>

namespace eyelib {
namespace audio {

namespace {
static ALCdevice* s_device = nullptr;
static ALCcontext* s_context = nullptr;
static int s_refCount = 0;
}

bool AudioDevice::Initialize() {
    if (m_initialized) return true;
    if (s_refCount == 0) {
        s_device = alcOpenDevice(nullptr);
        if (!s_device) {
            core::Log::Error("AudioDevice: Failed to open OpenAL device.");
            return false;
        }
        s_context = alcCreateContext(s_device, nullptr);
        if (!s_context || !alcMakeContextCurrent(s_context)) {
            core::Log::Error("AudioDevice: Failed to create OpenAL context.");
            if (s_context) alcDestroyContext(s_context);
            alcCloseDevice(s_device);
            s_context = nullptr;
            s_device = nullptr;
            return false;
        }
        alListenerf(AL_GAIN, m_masterVolume);
    }
    s_refCount++;
    m_initialized = true;
    core::Log::Info("AudioDevice initialized (OpenAL Soft).");
    return true;
}

void AudioDevice::Shutdown() {
    if (!m_initialized) return;
    m_initialized = false;
    s_refCount = std::max(0, s_refCount - 1);
    if (s_refCount == 0) {
        alcMakeContextCurrent(nullptr);
        if (s_context) alcDestroyContext(s_context);
        if (s_device) alcCloseDevice(s_device);
        s_context = nullptr;
        s_device = nullptr;
    }
    core::Log::Info("AudioDevice shutdown.");
}

void AudioDevice::SetMasterVolume(float volume) {
    m_masterVolume = (volume < 0.0f) ? 0.0f : volume;
    alListenerf(AL_GAIN, m_masterVolume);
}

} // namespace audio
} // namespace eyelib
