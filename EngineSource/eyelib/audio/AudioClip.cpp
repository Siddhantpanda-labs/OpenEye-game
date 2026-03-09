#include "eyelib/audio/AudioClip.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/assets/AudioClip.h"
#include "eyelib/assets/loaders/AudioLoader.h"

#include <algorithm>
#include <cstring>

namespace eyelib {
namespace audio {

namespace {
static bool ReadChunk(const uint8_t* data, size_t size, size_t offset, void* dst, size_t bytes) {
    if (!dst || offset + bytes > size) return false;
    std::memcpy(dst, data + offset, bytes);
    return true;
}

static uint32_t ReadU32(const uint8_t* data, size_t size, size_t offset, bool& ok) {
    uint32_t v = 0;
    ok = ReadChunk(data, size, offset, &v, sizeof(v));
    return v;
}

static uint16_t ReadU16(const uint8_t* data, size_t size, size_t offset, bool& ok) {
    uint16_t v = 0;
    ok = ReadChunk(data, size, offset, &v, sizeof(v));
    return v;
}
} // namespace

bool AudioClip::LoadFromFile(const std::string& path) {
    const assets::AssetId id = assets::AudioLoader::LoadAudio(path);
    auto clip = assets::AssetManager::Get().Cache().Get<assets::AudioClip>(id);
    if (!clip) return false;
    m_assetId = id;
    m_sampleRate = clip->sampleRate;
    m_channels = clip->channels;
    m_samples = clip->samples;
    return m_sampleRate > 0 && m_channels > 0 && !m_samples.empty();
}

bool AudioClip::LoadFromMemory(const uint8_t* data, size_t sizeBytes) {
    if (!data || sizeBytes < 12) return false;

    if (std::memcmp(data, "RIFF", 4) != 0) return false;
    if (std::memcmp(data + 8, "WAVE", 4) != 0) return false;

    size_t offset = 12;
    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    const uint8_t* pcmData = nullptr;
    size_t pcmSize = 0;

    while (offset + 8 <= sizeBytes) {
        char chunkId[4]{};
        if (!ReadChunk(data, sizeBytes, offset, chunkId, 4)) break;
        bool ok = false;
        uint32_t chunkSize = ReadU32(data, sizeBytes, offset + 4, ok);
        if (!ok) break;
        offset += 8;

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            audioFormat = ReadU16(data, sizeBytes, offset + 0, ok);
            if (!ok) return false;
            channels = ReadU16(data, sizeBytes, offset + 2, ok);
            if (!ok) return false;
            sampleRate = ReadU32(data, sizeBytes, offset + 4, ok);
            if (!ok) return false;
            bitsPerSample = ReadU16(data, sizeBytes, offset + 14, ok);
            if (!ok) return false;
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            pcmData = data + offset;
            pcmSize = chunkSize;
        }

        offset += chunkSize;
    }

    if (!pcmData || pcmSize == 0 || channels == 0 || sampleRate == 0) return false;

    m_channels = static_cast<int>(channels);
    m_sampleRate = static_cast<int>(sampleRate);

    if (audioFormat == 1 && bitsPerSample == 16) {
        const int16_t* samples = reinterpret_cast<const int16_t*>(pcmData);
        size_t count = pcmSize / sizeof(int16_t);
        m_samples.resize(count);
        for (size_t i = 0; i < count; ++i) {
            m_samples[i] = static_cast<float>(samples[i]) / 32768.0f;
        }
        return true;
    }

    if (audioFormat == 3 && bitsPerSample == 32) {
        const float* samples = reinterpret_cast<const float*>(pcmData);
        size_t count = pcmSize / sizeof(float);
        m_samples.assign(samples, samples + count);
        return true;
    }

    return false;
}

float AudioClip::GetDuration() const {
    if (m_sampleRate <= 0 || m_channels <= 0 || m_samples.empty()) return 0.0f;
    const size_t frames = m_samples.size() / static_cast<size_t>(m_channels);
    return static_cast<float>(frames) / static_cast<float>(m_sampleRate);
}

} // namespace audio
} // namespace eyelib
