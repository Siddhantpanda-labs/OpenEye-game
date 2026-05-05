#include "eyelib/audio/AudioSource.h"

#include "eyelib/core/Time.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

#define AL_ALEXT_PROTOTYPES
#define AL_EFX_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include "stb/stb_vorbis.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <wrl/client.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

namespace eyelib {
namespace audio {

namespace {
static ALenum GetFormat(int channels) {
    return (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
}

static std::vector<int16_t> ToInt16(const std::vector<float>& samples) {
    std::vector<int16_t> out;
    out.resize(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        float s = std::max(-1.0f, std::min(1.0f, samples[i]));
        out[i] = static_cast<int16_t>(s * 32767.0f);
    }
    return out;
}

static float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

#if defined(_WIN32)
static int s_mfRefCount = 0;
static int s_comRefCount = 0;
static bool EnsureMF(bool& didInit) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInit = SUCCEEDED(hr);
    if (comInit) s_comRefCount++;
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        if (comInit) {
            s_comRefCount = std::max(0, s_comRefCount - 1);
            CoUninitialize();
        }
        return false;
    }
    s_mfRefCount++;
    didInit = true;
    return true;
}

static void ShutdownMF(bool didInit) {
    if (!didInit) return;
    s_mfRefCount = std::max(0, s_mfRefCount - 1);
    MFShutdown();
    if (s_comRefCount > 0) {
        s_comRefCount = std::max(0, s_comRefCount - 1);
        CoUninitialize();
    }
}

static std::wstring ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (size <= 0) return std::wstring();
    std::wstring out(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), size);
    return out;
}
#endif

static std::string Lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

static bool LoadWavHeader(const std::string& path, AudioSource::StreamState& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    auto readU32 = [&]() {
        uint32_t v = 0;
        file.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
    };
    auto readU16 = [&]() {
        uint16_t v = 0;
        file.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
    };

    char riff[4];
    file.read(riff, 4);
    if (std::string(riff, 4) != "RIFF") return false;
    (void)readU32();
    char wave[4];
    file.read(wave, 4);
    if (std::string(wave, 4) != "WAVE") return false;

    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;

    while (file && !file.eof()) {
        char chunkId[4];
        file.read(chunkId, 4);
        if (file.gcount() < 4) break;
        uint32_t chunkSize = readU32();
        std::string id(chunkId, 4);
        if (id == "fmt ") {
            audioFormat = readU16();
            channels = readU16();
            sampleRate = readU32();
            (void)readU32();
            (void)readU16();
            bitsPerSample = readU16();
            file.seekg(static_cast<std::streamoff>(chunkSize) - 16, std::ios::cur);
        } else if (id == "data") {
            dataOffset = static_cast<uint32_t>(file.tellg());
            dataSize = chunkSize;
            file.seekg(chunkSize, std::ios::cur);
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (dataSize == 0 || channels == 0 || sampleRate == 0) return false;
    if (audioFormat != 1 && audioFormat != 3) return false;
    if (bitsPerSample != 16 && bitsPerSample != 32) return false;

    out.path = path;
    out.dataOffset = dataOffset;
    out.dataSize = dataSize;
    out.cursor = 0;
    out.sampleRate = static_cast<int>(sampleRate);
    out.channels = static_cast<int>(channels);
    out.bitsPerSample = bitsPerSample;
    return true;
}

static bool LoadStreamHeader(const std::string& path, AudioSource::StreamState& out) {
    const std::string lower = Lower(path);
    if (lower.rfind(".ogg") != std::string::npos) {
        int error = 0;
        stb_vorbis* vorbis = stb_vorbis_open_filename(path.c_str(), &error, nullptr);
        if (!vorbis) return false;
        stb_vorbis_info info = stb_vorbis_get_info(vorbis);
        out.path = path;
        out.sampleRate = info.sample_rate;
        out.channels = info.channels;
        out.bitsPerSample = 16;
        out.dataOffset = 0;
        out.dataSize = 0;
        out.cursor = 0;
        out.isOgg = true;
        out.oggHandle = vorbis;
        return true;
    }
#if defined(_WIN32)
    if (lower.rfind(".mp3") != std::string::npos) {
        bool mfInit = false;
        if (!EnsureMF(mfInit)) return false;

        Microsoft::WRL::ComPtr<IMFSourceReader> reader;
        std::wstring wpath = ToWide(path);
        HRESULT hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader);
        if (FAILED(hr)) {
            ShutdownMF(mfInit);
            return false;
        }

        Microsoft::WRL::ComPtr<IMFMediaType> type;
        hr = MFCreateMediaType(&type);
        if (FAILED(hr)) {
            ShutdownMF(mfInit);
            return false;
        }
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
        type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 0);
        reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, type.Get());

        Microsoft::WRL::ComPtr<IMFMediaType> actual;
        hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actual);
        if (FAILED(hr)) {
            ShutdownMF(mfInit);
            return false;
        }

        UINT32 channels = 0;
        UINT32 sampleRate = 0;
        UINT32 bitsPerSample = 0;
        actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
        actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
        actual->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
        if (channels == 0 || sampleRate == 0 || bitsPerSample != 16) {
            ShutdownMF(mfInit);
            return false;
        }

        out.path = path;
        out.sampleRate = static_cast<int>(sampleRate);
        out.channels = static_cast<int>(channels);
        out.bitsPerSample = static_cast<uint16_t>(bitsPerSample);
        out.dataOffset = 0;
        out.dataSize = 0;
        out.cursor = 0;
        out.isMp3 = true;
        out.mfInit = mfInit;
        out.mfReader = reader.Detach();
        return true;
    }
#endif
    out.isOgg = false;
    out.oggHandle = nullptr;
    out.isMp3 = false;
    out.mfReader = nullptr;
    return LoadWavHeader(path, out);
}

static size_t ReadStreamChunk(AudioSource::StreamState& st, std::vector<int16_t>& outPcm) {
    if (st.isOgg) {
        if (!st.oggHandle) return 0;
        const int frames = 4096;
        outPcm.resize(static_cast<size_t>(frames) * static_cast<size_t>(st.channels));
        int framesRead = stb_vorbis_get_samples_short_interleaved(
            static_cast<stb_vorbis*>(st.oggHandle),
            st.channels,
            outPcm.data(),
            static_cast<int>(outPcm.size()));
        if (framesRead <= 0) return 0;
        const size_t count = static_cast<size_t>(framesRead) * static_cast<size_t>(st.channels);
        outPcm.resize(count);
        return count;
    }
#if defined(_WIN32)
    if (st.isMp3 && st.mfReader) {
        IMFSourceReader* reader = static_cast<IMFSourceReader*>(st.mfReader);
        DWORD flags = 0;
        Microsoft::WRL::ComPtr<IMFSample> sample;
        HRESULT hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) return 0;
        if (!sample) return 0;

        Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) return 0;

        BYTE* data = nullptr;
        DWORD maxLen = 0;
        DWORD curLen = 0;
        if (FAILED(buffer->Lock(&data, &maxLen, &curLen))) return 0;

        const size_t count = curLen / sizeof(int16_t);
        outPcm.resize(count);
        std::memcpy(outPcm.data(), data, count * sizeof(int16_t));
        buffer->Unlock();
        return count;
    }
#endif

    if (st.cursor >= st.dataSize) return 0;
    const size_t bytesPerSample = (st.bitsPerSample / 8);
    const size_t frameBytes = bytesPerSample * static_cast<size_t>(st.channels);
    const size_t maxBytes = 4096 * frameBytes;
    const uint32_t remaining = st.dataSize - st.cursor;
    const size_t toRead = std::min<size_t>(remaining, maxBytes);
    if (toRead == 0) return 0;

    std::ifstream file(st.path, std::ios::binary);
    if (!file.is_open()) return 0;
    file.seekg(st.dataOffset + st.cursor, std::ios::beg);
    std::vector<uint8_t> raw(toRead);
    file.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(toRead));
    const size_t readBytes = static_cast<size_t>(file.gcount());
    st.cursor += static_cast<uint32_t>(readBytes);

    if (st.bitsPerSample == 16) {
        const size_t count = readBytes / sizeof(int16_t);
        outPcm.resize(count);
        std::memcpy(outPcm.data(), raw.data(), count * sizeof(int16_t));
        return count;
    }

    const size_t count = readBytes / sizeof(float);
    outPcm.resize(count);
    const float* src = reinterpret_cast<const float*>(raw.data());
    for (size_t i = 0; i < count; ++i) {
        float s = std::max(-1.0f, std::min(1.0f, src[i]));
        outPcm[i] = static_cast<int16_t>(s * 32767.0f);
    }
    return count;
}
} // namespace

AudioSource::AudioSource() = default;

AudioSource::~AudioSource() {
    if (m_streaming) {
        alSourceStop(m_alSource);
        ALint queued = 0;
        alGetSourcei(m_alSource, AL_BUFFERS_QUEUED, &queued);
        while (queued-- > 0) {
            ALuint b = 0;
            alSourceUnqueueBuffers(m_alSource, 1, &b);
        }
        if (m_stream.bufferCount > 0) {
            alDeleteBuffers(static_cast<ALsizei>(m_stream.bufferCount), m_stream.buffers);
        }
        if (m_stream.isOgg && m_stream.oggHandle) {
            stb_vorbis_close(static_cast<stb_vorbis*>(m_stream.oggHandle));
            m_stream.oggHandle = nullptr;
        }
        if (m_stream.isMp3 && m_stream.mfReader) {
#if defined(_WIN32)
            static_cast<IMFSourceReader*>(m_stream.mfReader)->Release();
#endif
            m_stream.mfReader = nullptr;
            ShutdownMF(m_stream.mfInit);
        }
        m_streaming = false;
    }
    if (m_alSource) {
        alSourceStop(m_alSource);
        alDeleteSources(1, &m_alSource);
        m_alSource = 0;
    }
    if (m_alBuffer) {
        alDeleteBuffers(1, &m_alBuffer);
        m_alBuffer = 0;
    }
    if (m_reverbSlot) {
        alDeleteAuxiliaryEffectSlots(1, &m_reverbSlot);
        m_reverbSlot = 0;
    }
    if (m_reverbEffect) {
        alDeleteEffects(1, &m_reverbEffect);
        m_reverbEffect = 0;
    }
    if (m_echoSlot) {
        alDeleteAuxiliaryEffectSlots(1, &m_echoSlot);
        m_echoSlot = 0;
    }
    if (m_echoEffect) {
        alDeleteEffects(1, &m_echoEffect);
        m_echoEffect = 0;
    }
    if (m_filter) {
        alDeleteFilters(1, &m_filter);
        m_filter = 0;
    }
}

void AudioSource::Play(const AudioClip& clip) {
    m_clip = &clip;
    m_playing = true;
    m_paused = false;
    m_offset = 0.0f;
    m_startTime = core::Time::TotalTime();
    m_streaming = false;

    if (!m_alSource) {
        alGenSources(1, &m_alSource);
    }
    if (m_alBuffer) {
        alDeleteBuffers(1, &m_alBuffer);
        m_alBuffer = 0;
    }
    alGenBuffers(1, &m_alBuffer);

    const auto& samples = clip.GetSamples();
    const int channels = clip.GetChannels();
    const int sampleRate = clip.GetSampleRate();
    if (!samples.empty() && sampleRate > 0 && (channels == 1 || channels == 2)) {
        const auto pcm = ToInt16(samples);
        alBufferData(m_alBuffer, GetFormat(channels), pcm.data(),
                     static_cast<ALsizei>(pcm.size() * sizeof(int16_t)),
                     static_cast<ALsizei>(sampleRate));
        alSourcei(m_alSource, AL_BUFFER, static_cast<ALint>(m_alBuffer));
    }

    alSourcef(m_alSource, AL_GAIN, m_volume * m_groupVolume);
    alSourcef(m_alSource, AL_PITCH, m_pitch);
    alSourcei(m_alSource, AL_LOOPING, m_looping ? AL_TRUE : AL_FALSE);
    alSource3f(m_alSource, AL_POSITION, m_position.x, m_position.y, m_position.z);
    alSource3f(m_alSource, AL_VELOCITY, m_velocity.x, m_velocity.y, m_velocity.z);
    alSourcePlay(m_alSource);
}

void AudioSource::Stop() {
    m_playing = false;
    m_paused = false;
    m_offset = 0.0f;
    if (m_alSource) alSourceStop(m_alSource);
    if (m_streaming) {
        ALint queued = 0;
        alGetSourcei(m_alSource, AL_BUFFERS_QUEUED, &queued);
        while (queued-- > 0) {
            ALuint b = 0;
            alSourceUnqueueBuffers(m_alSource, 1, &b);
        }
        if (m_stream.bufferCount > 0) {
            alDeleteBuffers(static_cast<ALsizei>(m_stream.bufferCount), m_stream.buffers);
        }
        if (m_stream.isOgg && m_stream.oggHandle) {
            stb_vorbis_close(static_cast<stb_vorbis*>(m_stream.oggHandle));
        }
        if (m_stream.isMp3 && m_stream.mfReader) {
#if defined(_WIN32)
            static_cast<IMFSourceReader*>(m_stream.mfReader)->Release();
#endif
            ShutdownMF(m_stream.mfInit);
        }
        m_stream = {};
        m_streaming = false;
    }
}

void AudioSource::Pause() {
    if (!m_playing || m_paused) return;
    m_offset = GetTime();
    m_paused = true;
    if (m_alSource) alSourcePause(m_alSource);
}

void AudioSource::SetVolume(float volume) {
    m_volume = std::max(0.0f, volume);
    if (m_alSource) alSourcef(m_alSource, AL_GAIN, m_volume * m_groupVolume);
}

void AudioSource::SetPitch(float pitch) {
    m_pitch = std::max(0.0f, pitch);
    if (m_alSource) alSourcef(m_alSource, AL_PITCH, m_pitch);
}

void AudioSource::SetLooping(bool loop) {
    m_looping = loop;
    if (m_alSource) alSourcei(m_alSource, AL_LOOPING, m_looping ? AL_TRUE : AL_FALSE);
}

void AudioSource::SetGroupVolume(float volume) {
    m_groupVolume = std::max(0.0f, volume);
    if (m_alSource) alSourcef(m_alSource, AL_GAIN, m_volume * m_groupVolume);
}

void AudioSource::SetPosition(const math::Vec3& position) {
    m_position = position;
    if (m_alSource) alSource3f(m_alSource, AL_POSITION, m_position.x, m_position.y, m_position.z);
}

void AudioSource::SetVelocity(const math::Vec3& velocity) {
    m_velocity = velocity;
    if (m_alSource) alSource3f(m_alSource, AL_VELOCITY, m_velocity.x, m_velocity.y, m_velocity.z);
}

bool AudioSource::IsPlaying() const {
    if (!m_playing || !m_clip) return false;
    if (m_alSource) {
        ALint state = 0;
        alGetSourcei(m_alSource, AL_SOURCE_STATE, &state);
        return state == AL_PLAYING;
    }
    if (m_looping) return true;
    return GetTime() < m_clip->GetDuration();
}

float AudioSource::GetTime() const {
    if (!m_playing || !m_clip) return 0.0f;
    if (m_alSource) {
        float sec = 0.0f;
        alGetSourcef(m_alSource, AL_SEC_OFFSET, &sec);
        return sec;
    }
    float t = m_offset;
    if (!m_paused) {
        t += core::Time::TotalTime() - m_startTime;
    }
    if (!m_looping) {
        t = std::min(t, m_clip->GetDuration());
    } else if (m_clip->GetDuration() > 0.0f) {
        t = std::fmod(t, m_clip->GetDuration());
    }
    return t;
}

bool AudioSource::PlayStream(const std::string& path, bool loop) {
    Stop();
    if (!LoadStreamHeader(path, m_stream)) return false;

    m_stream.loop = loop;
    m_streaming = true;
    m_playing = true;
    m_paused = false;
    m_offset = 0.0f;
    m_startTime = core::Time::TotalTime();

    if (!m_alSource) {
        alGenSources(1, &m_alSource);
    }
    alSourcei(m_alSource, AL_LOOPING, AL_FALSE);
    alSourcef(m_alSource, AL_GAIN, m_volume * m_groupVolume);
    alSourcef(m_alSource, AL_PITCH, m_pitch);
    alSource3f(m_alSource, AL_POSITION, m_position.x, m_position.y, m_position.z);
    alSource3f(m_alSource, AL_VELOCITY, m_velocity.x, m_velocity.y, m_velocity.z);

    alGenBuffers(4, m_stream.buffers);
    m_stream.bufferCount = 4;

    std::vector<int16_t> pcm;
    const ALenum format = GetFormat(m_stream.channels);
    for (size_t i = 0; i < m_stream.bufferCount; ++i) {
        const size_t count = ReadStreamChunk(m_stream, pcm);
        if (count == 0) break;
        alBufferData(m_stream.buffers[i], format, pcm.data(),
                     static_cast<ALsizei>(count * sizeof(int16_t)),
                     static_cast<ALsizei>(m_stream.sampleRate));
        alSourceQueueBuffers(m_alSource, 1, &m_stream.buffers[i]);
    }
    alSourcePlay(m_alSource);
    return true;
}

void AudioSource::UpdateStream() {
    if (!m_streaming || !m_alSource) return;

    ALint processed = 0;
    alGetSourcei(m_alSource, AL_BUFFERS_PROCESSED, &processed);
    if (processed <= 0) return;

    std::vector<int16_t> pcm;
    const ALenum format = GetFormat(m_stream.channels);
    while (processed-- > 0) {
        ALuint buffer = 0;
        alSourceUnqueueBuffers(m_alSource, 1, &buffer);
        const size_t count = ReadStreamChunk(m_stream, pcm);
        if (count == 0) {
            if (m_stream.loop) {
                if (m_stream.isOgg && m_stream.oggHandle) {
                    stb_vorbis_seek_start(static_cast<stb_vorbis*>(m_stream.oggHandle));
#if defined(_WIN32)
                } else if (m_stream.isMp3 && m_stream.mfReader) {
                    IMFSourceReader* reader = static_cast<IMFSourceReader*>(m_stream.mfReader);
                    PROPVARIANT pos;
                    PropVariantInit(&pos);
                    pos.vt = VT_I8;
                    pos.hVal.QuadPart = 0;
                    reader->SetCurrentPosition(GUID_NULL, pos);
                    PropVariantClear(&pos);
#endif
                } else {
                    m_stream.cursor = 0;
                }
                const size_t retry = ReadStreamChunk(m_stream, pcm);
                if (retry == 0) continue;
                alBufferData(buffer, format, pcm.data(),
                             static_cast<ALsizei>(retry * sizeof(int16_t)),
                             static_cast<ALsizei>(m_stream.sampleRate));
                alSourceQueueBuffers(m_alSource, 1, &buffer);
            }
            continue;
        }
        alBufferData(buffer, format, pcm.data(),
                     static_cast<ALsizei>(count * sizeof(int16_t)),
                     static_cast<ALsizei>(m_stream.sampleRate));
        alSourceQueueBuffers(m_alSource, 1, &buffer);
    }

    ALint state = 0;
    alGetSourcei(m_alSource, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) {
        alSourcePlay(m_alSource);
    }
}

void AudioSource::ApplyReverb(const ReverbSettings& settings) {
    if (!m_reverbEffect) {
        alGenEffects(1, &m_reverbEffect);
        alEffecti(m_reverbEffect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
    }
    if (!m_reverbSlot) {
        alGenAuxiliaryEffectSlots(1, &m_reverbSlot);
    }
    alEffectf(m_reverbEffect, AL_REVERB_DECAY_TIME, 0.1f + settings.roomSize * 4.0f);
    alEffectf(m_reverbEffect, AL_REVERB_DECAY_HFRATIO, 0.1f + settings.damping * 1.9f);
    alEffectf(m_reverbEffect, AL_REVERB_GAIN, Clamp01(settings.dryLevel));
    alEffectf(m_reverbEffect, AL_REVERB_GAINHF, Clamp01(settings.wetLevel));
    alAuxiliaryEffectSloti(m_reverbSlot, AL_EFFECTSLOT_EFFECT, m_reverbEffect);
    if (m_alSource) {
        alSource3i(m_alSource, AL_AUXILIARY_SEND_FILTER, m_reverbSlot, 0, AL_FILTER_NULL);
    }
}

void AudioSource::ApplyEcho(const EchoSettings& settings) {
    if (!m_echoEffect) {
        alGenEffects(1, &m_echoEffect);
        alEffecti(m_echoEffect, AL_EFFECT_TYPE, AL_EFFECT_ECHO);
    }
    if (!m_echoSlot) {
        alGenAuxiliaryEffectSlots(1, &m_echoSlot);
    }
    alEffectf(m_echoEffect, AL_ECHO_DELAY, std::max(0.0f, settings.delay));
    alEffectf(m_echoEffect, AL_ECHO_FEEDBACK, Clamp01(settings.feedback));
    alEffectf(m_echoEffect, AL_ECHO_DAMPING, 0.5f);
    alAuxiliaryEffectSloti(m_echoSlot, AL_EFFECTSLOT_EFFECT, m_echoEffect);
    if (m_alSource) {
        alSource3i(m_alSource, AL_AUXILIARY_SEND_FILTER, m_echoSlot, 1, AL_FILTER_NULL);
    }
}

void AudioSource::ApplyFilter(const FilterSettings& settings) {
    if (!m_filter) {
        alGenFilters(1, &m_filter);
    }
    ALenum type = AL_FILTER_LOWPASS;
    switch (settings.type) {
        case FilterType::HighPass: type = AL_FILTER_HIGHPASS; break;
        case FilterType::BandPass: type = AL_FILTER_BANDPASS; break;
        default: type = AL_FILTER_LOWPASS; break;
    }
    alFilteri(m_filter, AL_FILTER_TYPE, type);
    const float gain = Clamp01(1.0f);
    const float hfGain = Clamp01(settings.cutoff / 20000.0f);
    if (type == AL_FILTER_LOWPASS) {
        alFilterf(m_filter, AL_LOWPASS_GAIN, gain);
        alFilterf(m_filter, AL_LOWPASS_GAINHF, hfGain);
    } else if (type == AL_FILTER_HIGHPASS) {
        alFilterf(m_filter, AL_HIGHPASS_GAIN, gain);
        alFilterf(m_filter, AL_HIGHPASS_GAINLF, Clamp01(settings.resonance));
    } else {
        alFilterf(m_filter, AL_BANDPASS_GAIN, gain);
        alFilterf(m_filter, AL_BANDPASS_GAINHF, hfGain);
        alFilterf(m_filter, AL_BANDPASS_GAINLF, Clamp01(settings.resonance));
    }
    if (m_alSource) {
        alSourcei(m_alSource, AL_DIRECT_FILTER, static_cast<ALint>(m_filter));
    }
}

} // namespace audio
} // namespace eyelib
