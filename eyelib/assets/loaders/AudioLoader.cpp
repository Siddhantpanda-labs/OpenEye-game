#include "AudioLoader.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/assets/AudioClip.h"
#include "eyelib/core/Log.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <thread>
#include <string>
#include <vector>

#include "stb/stb_vorbis.c"

namespace eyelib {
namespace assets {

namespace {
std::string Lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool LoadWav(const std::string& path, AudioClip& clip) {
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
    (void)readU32(); // file size
    char wave[4];
    file.read(wave, 4);
    if (std::string(wave, 4) != "WAVE") return false;

    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    std::vector<uint8_t> data;

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
            (void)readU32(); // byte rate
            (void)readU16(); // block align
            bitsPerSample = readU16();
            file.seekg(static_cast<std::streamoff>(chunkSize) - 16, std::ios::cur);
        } else if (id == "data") {
            data.resize(chunkSize);
            file.read(reinterpret_cast<char*>(data.data()), chunkSize);
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (data.empty() || channels == 0 || sampleRate == 0) return false;

    clip.channels = static_cast<int>(channels);
    clip.sampleRate = static_cast<int>(sampleRate);

    if (audioFormat == 1 && bitsPerSample == 16) {
        const int16_t* samples = reinterpret_cast<const int16_t*>(data.data());
        size_t count = data.size() / sizeof(int16_t);
        clip.samples.resize(count);
        for (size_t i = 0; i < count; ++i) {
            clip.samples[i] = static_cast<float>(samples[i]) / 32768.0f;
        }
        return true;
    }

    if (audioFormat == 3 && bitsPerSample == 32) {
        const float* samples = reinterpret_cast<const float*>(data.data());
        size_t count = data.size() / sizeof(float);
        clip.samples.assign(samples, samples + count);
        return true;
    }

    return false;
}

bool LoadOgg(const std::string& path, AudioClip& clip) {
    int channels = 0;
    int sampleRate = 0;
    short* output = nullptr;
    int samples = stb_vorbis_decode_filename(path.c_str(), &channels, &sampleRate, &output);
    if (samples <= 0 || !output) return false;
    clip.channels = channels;
    clip.sampleRate = sampleRate;
    clip.samples.resize(static_cast<size_t>(samples) * channels);
    for (int i = 0; i < samples * channels; ++i) {
        clip.samples[i] = static_cast<float>(output[i]) / 32768.0f;
    }
    free(output);
    return true;
}
} // namespace

AssetId AudioLoader::LoadAudio(const std::string& path) {
    auto& manager = AssetManager::Get();
    const AssetId id = manager.Registry().RegisterAsset(path, "Audio");

    AudioClip clip;
    const std::string lower = Lower(path);
    bool ok = false;
    if (lower.rfind(".wav") != std::string::npos) {
        ok = LoadWav(path, clip);
    } else if (lower.rfind(".ogg") != std::string::npos) {
        ok = LoadOgg(path, clip);
    } else if (lower.rfind(".mp3") != std::string::npos) {
        core::Log::Warn("AudioLoader: MP3 not supported yet.");
    }

    if (ok) {
        manager.Cache().Store(id, std::make_shared<AudioClip>(std::move(clip)), "Audio");
    }
    return id;
}

void AudioLoader::LoadAudioAsync(const std::string& path, LoadCallback callback) {
    std::thread([path, callback]() {
        const AssetId id = LoadAudio(path);
        if (callback) callback(id);
    }).detach();
}

} // namespace assets
} // namespace eyelib

